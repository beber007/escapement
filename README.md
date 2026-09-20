# Escapement

[![build](https://github.com/beber007/escapement/actions/workflows/build.yml/badge.svg)](https://github.com/beber007/escapement/actions/workflows/build.yml)

**Lightweight Power-Aware Real-Time OS** pour microcontrôleurs ARM Cortex-M et TI MSP430.

Escapement est un noyau temps réel préemptif à ordonnancement par échéances (EDF),
conçu pour des cibles de quelques kilo-octets de RAM. Sa variante *power-aware*
ajuste dynamiquement la tension et la fréquence cœur en fonction de la charge
temps réel, de façon à consommer le minimum d'énergie tout en garantissant les
échéances.

> Le nom vient de l'échappement horloger : la pièce qui libère l'énergie du
> barillet par incréments réguliers — exactement ce que fait un ordonnanceur
> temps réel économe.

## Variantes du noyau

| Variante | Fichiers | Usage |
|---|---|---|
| **Hard** | `Escapement/EscapementHard.{c,h}` | Temps réel dur : échéances garanties |
| **Soft** | `Escapement/EscapementSoft.{c,h}` | Temps réel souple : tâches apériodiques tolérant le dépassement |
| **Hard PA** | `Escapement/EscapementHardPA.{c,h}` | Temps réel dur + gestion dynamique de l'énergie (DVFS) |

## Cibles supportées

- **ARM Cortex-M0 / M3 / M4** — portage dans `Escapement/CORTEX-Mx/`,
  *cible de développement*. Les bibliothèques ST sont fournies pour les
  familles STM32F0, F1, F2, F4 et L1 ; quatre cartes ont un exemple
  construit en CI (voir « Compilation »), la F2 n'en a pas.
- **TI MSP430** — MSP430x1xx à x5xx, MSP430FR57xx, CC430. Portage dans
  `Escapement/msp430/`. *Gelé*, voir « Orientation » ci-dessous. **Il n'existe
  aucun build pour cette cible** : les projets d'origine étaient des projets
  IAR / Code Composer, absents du dépôt.

## Arborescence

```
Escapement/
  EscapementHard.{c,h}      noyau temps réel dur
  EscapementSoft.{c,h}      noyau temps réel souple
  EscapementHardPA.{c,h}    noyau temps réel dur power-aware
  CORTEX-Mx/                portage ARM (STM32, CMSIS, StdPeriph)
  msp430/                   portage MSP430
  CORTEX-Mx/STM32/Examples/ quatre exemples avec Makefile
PA/                         exemple power-aware (MSP430F5419A)
USB/                        exemple avec pile USB (MSP430x552x)
Balls/                      démo graphique (MSP-EXP430F5438)
.github/workflows/build.yml compilation des quatre exemples STM32
```

## Compilation

Toolchain bare-metal ARM :

```sh
brew install arm-none-eabi-gcc         # macOS
sudo apt install gcc-arm-none-eabi     # Debian / Ubuntu
```

Chaque exemple se construit depuis son répertoire :

```sh
cd Escapement/CORTEX-Mx/STM32/Examples/stm32f0-discovery
make
```

Les quatre exemples sont construits à chaque push par la CI :

| Exemple | Cœur | MCU | Cibles | Noyau + une tâche périodique |
|---|---|---|---|---|
| `stm32f4-discovery` | Cortex-M4 | STM32F407VG | 3 |
| `stm32l-discovery-pa` | Cortex-M3 | STM32L152RB | 1 (*power-aware*) | 5 936 / 480 / 32 |
| `stm32l-discovery` | Cortex-M3 | STM32L152RB | 4 | 6 112 / 8 / 212 |
| `stm32vl-discovery` | Cortex-M3 | STM32F103RC | 3 | 7 792 / 8 / 272 |
| `stm32f0-discovery` | Cortex-M0 | STM32F051R8 | 4 | 8 884 / 8 / 160 |

Dernière colonne : `text` / `data` / `bss` en octets pour la cible `TaskLED`,
soit le noyau complet plus une tâche périodique qui fait clignoter une LED.

Le chemin de la toolchain est surchargeable :
`make CROSS_COMPILE=/chemin/vers/arm-none-eabi-`.

Le code est compilé en *freestanding* et lié sans bibliothèque C
(`-nostdlib`), avec seulement `libgcc` pour les routines que le matériel ne
fournit pas (division entière sur Cortex-M0 et M3). Aucune newlib n'est
nécessaire.

> **Aucun binaire n'a été exécuté sur du matériel.** La CI prouve qu'ils se
> construisent et que chaque configuration est cohérente avec son script de
> link, pas qu'ils tournent. Le MCU de `stm32vl-discovery` est d'ailleurs une
> déduction : son `Escapement_Config.h` désignait un STM32L152 depuis 2012
> alors que le seul script de link livré vise un STM32F103RC.

Le flashage se fait via OpenOCD (`openocd.cfg` fourni dans
`Escapement/CORTEX-Mx/STM32/Examples/`).

## Émulation

Le noyau **tourne** sur un STM32F407VG émulé par [Renode](https://renode.io) :

```sh
cd Escapement/CORTEX-Mx/STM32/Examples/stm32f4-discovery && make bin && cd -
renode emulation/renode/escapement_f4.resc
(monitor) sysbus LogPeripheralAccess sysbus.gpioPortB true
(monitor) emulation RunFor "1"
```

Deux tests Robot rejouent cette exécution à chaque push, dans le job
`emulation` de la CI :

```sh
pip install robotframework==6.1 robotframework-retryfailed psutil pyyaml
renode-test emulation/renode/escapement_f4.robot
```

| Test | Ce qu'il prouve |
|---|---|
| Les trois tâches périodiques sont ordonnancées | chacune des trois tâches allume **et** éteint sa sortie dans sa fenêtre temporelle |
| L'écho UART répond | le noyau ordonnance aussi le traitement piloté par interruption |

`TaskLEDF4` crée trois tâches périodiques de 100, 200 et 600 tops, qui
basculent chacune une sortie de GPIOB. Sur une seconde émulée :

| Sortie | Période | Basculements | Ratio mesuré | Ratio théorique |
|---|---:|---:|---:|---:|
| PB13 | 100 | 1220 | 5,98 | 6,00 |
| PB14 | 200 | 610 | 2,99 | 3,00 |
| PB15 | 600 | 204 | 1,00 | 1,00 |

L'ordonnancement par échéances respecte les périodes déclarées. C'est la
première exécution vérifiée du noyau depuis la reprise du projet.

### Le modèle de timer de Renode a dû être corrigé

Sans correction, le noyau se bloquait sur son propre garde-fou de surcharge.
La cause était dans `Timers.STM32_Timer`, au registre `EventGeneration` :

```csharp
.WithFlag(0, FieldMode.WriteOneToClear, writeCallback: (_, val) =>
{
    if(updateDisable.Value) { return; }   // <- aucun test sur val
    ...
    updateInterruptFlag = true;
}, name: "Update generation (UG)")
.WithTag("Capture/compare 1 generation (CC1G)", 1, 1)
```

Le callback du bit `UG` s'exécutait **quel que soit le bit écrit**. Écrire
`CC1G` — ce que fait `_OSStartTimer` pour forcer sa première interruption de
comparaison — générait donc un événement *update* et levait `UIF` au lieu de
`CC1IF`. Le noyau y lisait un débordement de compteur et décalait tous ses
temps de −2³⁰ à `CNT = 1`, rendant chaque tâche perpétuellement en retard.

Reproducteur minimal, hors de tout noyau :

| Séquence | Attendu | Renode 1.17.0 |
|---|---|---|
| `DIER = 0`, `EGR <- 0x2` | `SR = 0x0` | `SR = 0x0` |
| `DIER = 3`, `EGR <- 0x2` | `SR = 0x2` (`CC1IF`) | `SR = 0x1` (`UIF`) |

`emulation/renode/Escapement_STM32_Timer.cs` est une copie du modèle
d'origine (MIT, Antmicro) avec deux corrections : le callback `UG` est gardé
par un test sur la valeur écrite, et `CC1G` à `CC4G` sont implémentés. Renode
compile ce greffon à chaud, il n'y a rien à reconstruire. La plateforme CPU
est dérivée dans `stm32f4_escapement_cpu.repl` — Renode n'autorisant pas de
redéclarer un nœud, il faut copier le fichier pour changer le type de TIM2.

**Les deux correctifs sont à proposer en amont chez Antmicro.**

QEMU a été essayé d'abord (`-machine netduinoplus2`) : le noyau démarre mais
son timer n'est jamais réveillé, deux exceptions en 60 secondes.

## Power-aware

`Escapement/CORTEX-Mx/STM32/Escapement_Processor.c` est un pilote DVFS pour
STM32L1 : trois paliers à 4, 16 et 32 MHz, avec réglage de la tension cœur via
`PWR_CR`. L'exemple `stm32l-discovery-pa` l'active — même application et même
charge de 90 % que `stm32l-discovery`, mais chaque tâche déclare son temps
d'exécution au pire cas, ce dont le noyau se sert pour abaisser la fréquence.

```sh
cd Escapement/CORTEX-Mx/STM32/Examples/stm32l-discovery-pa && make bin && cd -
renode emulation/renode/escapement_l1_pa.resc
```

**Ce qui marche** : la variante compile (7 018 octets contre 6 112 pour la
version Hard, soit ~900 octets de logique DVFS), elle démarre, le pilote écrit
`PWR_CR = 0x800` — le palier de tension 1,8 V — et les trois tâches exécutent
chacune une instance avant que le noyau rejoigne sa boucle de veille
spécifique PA, qui remonte la fréquence avant chaque `WFI`.

**Ce qui ne marche pas** : l'ordonnancement ne se maintient pas au-delà de la
première instance. Deux causes possibles, non départagées :

- Sur un STM32L152RB, `OS_IO_TIM5` n'existe pas, donc le noyau tombe en **mode
  timer 16 bits**, qui reconstitue la partie haute de l'horloge à partir de
  l'interruption de débordement. C'est un chemin que l'on n'a jamais validé —
  l'exemple `stm32f4` utilise le mode 32 bits.
- La plateforme L151 de Renode ne modélise **ni RCC ni PWR** : les écritures du
  pilote DVFS sont visibles mais sans effet. Le noyau croit changer de
  fréquence, l'horloge émulée ne bouge pas, et sa base de temps diverge.

La seconde limite est structurelle : **aucun émulateur ne validera du DVFS**,
puisqu'il faudrait modéliser l'effet d'un changement de tension sur la vitesse
réelle. Cette variante demandera du matériel.

## API

Une application se construit en cinq étapes, la dernière rendant la main au
noyau définitivement :

```c
void Task1(void *argument);

int main(void)
{
   /* (1) initialisations spécifiques au processeur                           */
   /* (2) initialisations applicatives                                        */
   /* (3) tâche périodique : période et échéance de 10000 ticks, argument 34  */
   OSCreateTask(Task1, 0, 10000, 10000, (void *)34);
   /* (4) source et fréquence d'horloge cœur — voir les exemples fournis      */
   return OSStartMultitasking(NULL, NULL);   /* (5) ne retourne jamais        */
}

void Task1(void *argument)
{
   /* corps de la tâche */
   OSEndTask();                              /* obligatoire en fin de tâche   */
}
```

L'interface complète est documentée dans les en-têtes `EscapementHard.h`,
`EscapementSoft.h` et `EscapementHardPA.h`.

## État du projet

Reprise et maintenance d'une base de code temps réel existante ; voir `NOTICE`
pour la filiation et les composants tiers.

### Orientation

L'effort de développement porte sur **ARM Cortex-M**, et en priorité sur les
STM32L4 / STM32U5 : leur scaling de tension cœur et leurs modes basse
consommation sont ce qui donne du sens à la variante power-aware. Aujourd'hui
`EscapementHardPA` est référencé par les deux portages, mais le seul exemple
DVFS qui fonctionne (`PA/`) cible un MSP430F5419A, et le pilote de tension cœur
`VCORE.c` est spécifique MSP430.

Le portage **MSP430 est gelé** : conservé, pas développé. Il ne pèse que
7 600 lignes, il porte la seule démonstration power-aware existante, et la FRAM
des MSP430FRxx (non volatile, adressable à l'octet, écriture quasi gratuite en
énergie) reste sans équivalent pour l'*intermittent computing* sous récupération
d'énergie. Son avenir sera tranché une fois l'exemple PA disponible sur
Cortex-M. À noter que TI n'ajoute plus de nouvelle famille MSP430 et oriente les
nouveaux designs vers MSPM0 (Cortex-M0+).

### Chantiers

- [x] Réparer les `Makefile` : les quatre exemples STM32 se construisent.
- [x] Compilation vérifiée en CI (`.github/workflows/build.yml`).
- [x] **Exécuter le noyau.** Les trois tâches de `TaskLEDF4` sont ordonnancées
      à leurs périodes sous Renode (voir « Émulation »).
- [ ] Proposer en amont les deux correctifs du `Timers.STM32_Timer` de Renode.
- [x] Émulation rejouée en CI avec `renode-test` : l'exécution du noyau est
      devenue un test de non-régression.
- [~] **Variante power-aware sur Cortex-M** : `stm32l-discovery-pa` compile et
      démarre, le pilote DVFS écrit bien la tension cœur. Mais l'ordonnancement
      ne se maintient pas — voir « Power-aware » ci-dessous.
- [ ] Proposer en amont les deux correctifs du `Timers.STM32_Timer` de Renode.
- [ ] Reconstituer la documentation utilisateur (le manuel et les notes de
      référence d'origine ont été retirés avec le rebranding).
- [ ] MSP430, seulement si le portage est réactivé : reconstituer le générateur
      de configuration qui produisait les en-têtes par dérivé
      (`Escapement_msp430xNNN.h`), absents du dépôt, et un système de build —
      il n'en existe aucun pour cette cible.

## Licence

Voir `LICENSE` et `NOTICE`.
