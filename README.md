# Escapement

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

- **ARM Cortex-M0 / M3 / M4** — familles STM32F0, STM32F1, STM32F2, STM32F4,
  STM32L1. Portage dans `Escapement/CORTEX-Mx/`. *Cible de développement.*
- **TI MSP430** — MSP430x1xx à x5xx, MSP430FR57xx, CC430. Portage dans
  `Escapement/msp430/`. *Gelé*, voir « Orientation » ci-dessous.

## Arborescence

```
Escapement/
  EscapementHard.{c,h}      noyau temps réel dur
  EscapementSoft.{c,h}      noyau temps réel souple
  EscapementHardPA.{c,h}    noyau temps réel dur power-aware
  CORTEX-Mx/                portage ARM (STM32, CMSIS, StdPeriph)
  msp430/                   portage MSP430
PA/                         exemple power-aware (MSP430F5419A)
USB/                        exemple avec pile USB (MSP430x552x)
Balls/                      démo graphique (MSP-EXP430F5438)
```

## Compilation

Toolchain bare-metal ARM requise :

```sh
brew install arm-none-eabi-gcc        # macOS
```

L'exemple `stm32f0-discovery` se construit tel quel :

```sh
cd Escapement/CORTEX-Mx/STM32/Examples/stm32f0-discovery
make
```

Il produit quatre binaires pour un STM32F051R8 (Cortex-M0) :

| Binaire | text | data | bss |
|---|---:|---:|---:|
| `TaskLEDF0.elf` | 8 884 | 8 | 160 |
| `UARTSimpleEchoF0.elf` | 10 896 | 8 | 156 |
| `TestTimerEventF0b.elf` | 11 932 | 10 | 160 |
| `TestTimerEventF0.elf` | 12 168 | 8 | 156 |

Le chemin de la toolchain est surchargeable :
`make CROSS_COMPILE=/chemin/vers/arm-none-eabi-`.

Le projet est compilé en *freestanding* et lié sans bibliothèque C
(`-nostdlib`), avec seulement `libgcc` pour les routines que le Cortex-M0 ne
sait pas faire en matériel (division entière). Il n'a donc besoin d'aucune
newlib.

Trois exemples sur quatre se construisent et sont vérifiés en CI :

| Exemple | Cœur | MCU | Binaires |
|---|---|---|---|
| `stm32f0-discovery` | Cortex-M0 | STM32F051R8 | 4 |
| `stm32l-discovery` | Cortex-M3 | STM32L152RB | 4 |
| `stm32f4-discovery` | Cortex-M4 | STM32F407VG | 3 |
| `stm32vl-discovery` | Cortex-M3 | **incohérent** | — |

`stm32vl-discovery` est hors CI : son `Escapement_Config.h` sélectionne un
STM32L152 alors que son script de link vise un STM32F103RC. Deux de ses cibles
se compilent malgré tout, mais contre les définitions du mauvais
microcontrôleur — les binaires produits ne tourneraient pas. Le reste de
l'exemple est réparé (sources restaurées, `Makefile`, appels d'API) ; il ne
manque que le choix du MCU visé.

Le flashage se fait via OpenOCD (`openocd.cfg` fourni dans
`Escapement/CORTEX-Mx/STM32/Examples/`).

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

- [ ] Porter la variante power-aware sur STM32L4 ou STM32U5, avec un exemple
      DVFS fonctionnel équivalent à `PA/`.
- [ ] Trancher le microcontrôleur visé par `stm32vl-discovery`, puis corriger
      son `Escapement_Config.h` et le remettre en CI.
- [x] Mettre en place une compilation vérifiable en CI (`.github/workflows/build.yml`).
- [ ] Reconstituer la documentation utilisateur (le manuel et les notes de
      référence d'origine ont été retirés avec le rebranding).
- [ ] MSP430, seulement si le portage est réactivé : reconstituer le générateur
      de configuration qui produisait les en-têtes par dérivé
      (`Escapement_msp430xNNN.h`), absents du dépôt.

## Licence

Voir `LICENSE` et `NOTICE`.
