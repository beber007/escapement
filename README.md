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
  STM32L1. Portage dans `Escapement/CORTEX-Mx/`.
- **TI MSP430** — MSP430x1xx à x5xx, MSP430FR57xx, CC430. Portage dans
  `Escapement/msp430/`.

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
docs/                       documentation
```

## Compilation

Les exemples STM32 se construisent avec `arm-none-eabi-gcc` :

```sh
cd Escapement/CORTEX-Mx/STM32/Examples/stm32f0-discovery
make
```

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
pour la filiation et les composants tiers. Chantiers en cours :

- [ ] Remplacer les chemins de toolchain codés en dur dans les `Makefile`
      (`CC = /root/CodeSourcery/...`).
- [ ] Reconstituer le générateur de configuration MSP430, qui produisait les
      en-têtes par dérivé (`Escapement_msp430xNNN.h`) absents du dépôt.
- [ ] Reconstituer la documentation utilisateur.
- [ ] Mettre en place une compilation vérifiable en CI.

## Licence

Voir `LICENSE` et `NOTICE`.
