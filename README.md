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
| `stm32f4-discovery` | Cortex-M4 | STM32F407VG | 3 | 5 936 / 480 / 32 |
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

Le noyau se lance sous [Renode](https://renode.io) sur un STM32F407VG :

```sh
cd Escapement/CORTEX-Mx/STM32/Examples/stm32f4-discovery && make bin && cd -
renode emulation/renode/escapement_f4.resc
(monitor) emulation RunFor "2"
```

**Ce qui est prouvé :** le noyau démarre, active les horloges, alloue par
`OSMalloc`, installe son vecteur d'interruption via `OSSetISRDescriptor`,
déplace `VTOR` vers `0x08000000`, et son interruption timer se déclenche.

**Où ça s'arrête :** dans `_OSTimerInterruptHandler`, sur un garde-fou du
noyau lui-même, actif en `DEBUG_MODE` :

```c
if (!(arrival->TaskState & STATE_ZOMBIE)) {
   _OSDisableInterrupts();
   while (TRUE); // If we get here, the processor utilization > 100%.
}
```

Une instance de tâche arrive alors que la précédente n'est pas terminée. Ce
n'est pas une question de vitesse : comportement identique à 500 et à 2000
MIPS émulés.

Le modèle `Timers.STM32_Timer` de Renode a été audité registre par registre.
Il est plus complet qu'attendu :

| Mécanisme | État |
|---|---|
| `ARR`, `PSC`, `CNT`, `CR1`, `DIER`, `SR` | conformes |
| `CCR1`, `CCMR1`, `CCER` | présents et relus correctement |
| Comparaison sur égalité → `CC1IF` | **fonctionne** |
| `CC1IF` → IRQ 28 au NVIC | **fonctionne** |
| Fréquence : 10 MHz déclarés | 10 MHz mesurés |
| `CC1G` (`EGR` bit 1), événement logiciel | **non implémenté** |

Renode le dit lui-même : `Unhandled write to offset 0x14. Unhandled bits: [1].
Tags: Capture/compare 1 generation`. Or c'est exactement ce bit qu'utilise
`_OSStartTimer` pour forcer la première interruption de comparaison et amorcer
l'ordonnanceur.

Émuler ce bit par un `AddWatchpointHook` qui arme `CCR1` juste devant le
compteur lève l'obstacle sans débloquer l'ordonnanceur pour autant.

### Pourquoi le garde-fou se déclenche

La séquence de démarrage a été instrumentée sous Renode, avec un point
d'arrêt sur le piège lui-même. Au moment où il se déclenche, le compteur vaut
`1` — c'est donc la toute première interruption — et la file d'arrivées
contient :

```
tcb=0x2001bf8c state=0x00 NextArrivalTimeLow=0xC00000C8
tcb=0x2001bf60 state=0x00 NextArrivalTimeLow=0xC0000258
```

`0xC00000C8` vaut `200 - 2^30`, `0xC0000258` vaut `600 - 2^30` : ce sont les
périodes des tâches, **décalées de −2³⁰**. Le noyau a appliqué son mécanisme
de recalage temporel (`ShiftTimeLimit`), qui n'a lieu que si
`_OSTimerIsOverflow()` est vrai — c'est-à-dire, pour un timer 32 bits, si
l'ISR bas niveau a vu le drapeau `UIF` de débordement. À `CNT = 1`, sur du
matériel réel, cette condition est impossible.

Tous les temps d'arrivée devenant très négatifs, chaque tâche est
perpétuellement « en retard » : la boucle de traitement des arrivées reprend
des tâches qu'elle vient de passer à `STATE_INIT`, et le garde-fou
`DEBUG_MODE` s'arme, à juste titre de son point de vue.

### L'origine du `UIF` parasite

Trouvée en traçant les accès au timer et l'activité du NVIC dans le même
journal :

```
[cpu: 0x328] Write Control1 = 0x1          CEN, le compteur démarre
[cpu: 0x336] Write EventGeneration = 0x2   CC1G
nvic: External IRQ 44: True                l'interruption part
timer2: Unhandled write to offset 0x14. Unhandled bits: [1]
[cpu: 0x34E] Read Status -> 0x1            l'ISR lit UIF, pas CC1IF
```

Renode ignore le bit `CC1G` mais génère quand même un événement, et c'est un
événement **update** : il lève `UIF` au lieu de `CC1IF`. Le noyau, qui
attendait sa première interruption de comparaison, reçoit un faux
débordement — d'où le recalage de −2³⁰ à `CNT = 1`.

Reproducteur minimal, hors de tout noyau :

| Séquence | Résultat |
|---|---|
| `DIER = 0` puis `EGR <- 0x2` | `SR = 0x0` |
| `DIER = 3` puis `EGR <- 0x2` | **`SR = 0x1`** (`UIF`) |

Sur un STM32 réel, `CC1G` lève `CC1IF` et jamais `UIF`. C'est un défaut du
modèle `Timers.STM32_Timer`, à signaler en amont.

Un contournement en deux parties a été tenté — réécrire le drapeau à la
lecture de `SR` au démarrage, et armer `CCR1` devant le compteur à l'écriture
de `CC1G`. Il s'exécute sans erreur mais ne suffit pas : le noyau atteint
toujours son garde-fou. Corriger le modèle en amont, ou passer par du
matériel, reste la voie propre.

QEMU a été essayé d'abord (`-machine netduinoplus2`) : le noyau démarre aussi,
mais son timer n'est jamais réveillé — deux exceptions en 60 secondes. Renode
va nettement plus loin.

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
- [ ] **Faire progresser l'ordonnanceur en émulation.** Voir « Émulation »
      ci-dessous : le noyau démarre et son interruption timer part, mais il
      s'arrête sur son propre garde-fou de surcharge.
- [ ] Porter la variante power-aware sur STM32L4 ou STM32U5, avec un exemple
      DVFS fonctionnel équivalent à `PA/`.
- [ ] Reconstituer la documentation utilisateur (le manuel et les notes de
      référence d'origine ont été retirés avec le rebranding).
- [ ] MSP430, seulement si le portage est réactivé : reconstituer le générateur
      de configuration qui produisait les en-têtes par dérivé
      (`Escapement_msp430xNNN.h`), absents du dépôt, et un système de build —
      il n'en existe aucun pour cette cible.

## Licence

Voir `LICENSE` et `NOTICE`.
