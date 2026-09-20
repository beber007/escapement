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
- **Raspberry Pi RP2040** — Cortex-M0+, portage dans `Escapement/CORTEX-Mx/RP2040/`.
  Timer 64 bits à quatre alarmes, cadencé **indépendamment de l'horloge cœur**.
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
| `RP2040/Examples/pico` | Cortex-M0+ | RP2040 | 1 |
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

Les trois tâches sont ordonnancées à leurs périodes, vérifié par un test Robot
en CI. Sur une seconde émulée :

| Sortie | Période | Basculements | Ratio mesuré | Ratio théorique |
|---|---:|---:|---:|---:|
| PB12 | 500 | 625 | 5,95 | 6,00 |
| PB13 | 1000 | 313 | 2,98 | 3,00 |
| PB14 | 3000 | 105 | 1,00 | 1,00 |

Le pilote DVFS s'exerce : **18 reprogrammations de la PLL** sur une demi-seconde.
La tension cœur, elle, reste au palier 1 — la boucle de veille de la variante PA
remonte à la vitesse maximale avant chaque `WFI`, et une charge de 90 % laisse
peu de marge pour descendre.

### Un troisième défaut de plateforme

Sur un STM32L152RB, `OS_IO_TIM5` n'existe pas : le noyau tombe donc en **mode
timer 16 bits**, où la partie haute de l'horloge est reconstituée à partir de
l'interruption de débordement. Or la plateforme L151 de Renode déclare TIM2
avec `initialLimit: 0xFFFFFFFF`, ce qui en fait un compteur **32 bits** — alors
que le TIM2 d'un STM32L1 est un timer 16 bits.

Conséquence : le compteur ne repassait jamais par zéro à 65 536, l'interruption
de débordement n'arrivait jamais, la partie haute du temps restait figée. Mesuré
directement : `CNT = 93 741` après 0,3 s, bien au-delà de ce qu'un compteur
16 bits peut atteindre. La plateforme dérivée corrige `initialLimit` à `0xFFFF`.

### Le DVFS vaut-il quelque chose sur un STM32 ?

Question ouverte, et il faut la poser honnêtement avant d'investir dans cette
variante.

**La physique.** Pour un travail fixe de *W* cycles, l'énergie dynamique vaut
`α·C·V²·f · W/f = α·C·V²·W` : elle **ne dépend pas de la fréquence**. Baisser
*f* seul ne fait rien gagner, et allonge même le temps actif donc l'énergie de
fuite. Le seul levier est **V²**, et on ne peut baisser V qu'en baissant f.

**Ce qui rend le STM32L1 intéressant.** Trois plages de régulateur — 1,8 V
jusqu'à 32 MHz, 1,5 V jusqu'à 16 MHz, 1,2 V jusqu'à 4,2 MHz. Soit
(1,8/1,2)² = **2,25×** en théorie sur l'énergie dynamique. Et la plage est
choisie par logiciel : la plupart des firmwares posent la plage 1 au démarrage
et n'y touchent plus, il y a donc de la marge inexploitée.

**Ce qui tempère fortement.** Le chiffre qui compte est le µA/MHz du datasheet,
et il ne suit pas la loi en V² : l'ordre de grandeur annoncé pour le STM32L1 est
d'environ 230 µA/MHz en plage 1 contre 185 à 200 en plage 3 — **chiffres à
vérifier sur le datasheet ST**, ils ne sont pas issus d'une mesure faite ici.
Soit de l'ordre de 20 % de gain par cycle, loin du 2,25× théorique. La loi en V²
ne s'applique qu'à la commutation ; le régulateur, la flash, l'analogique
toujours alimenté et les fuites sont incompressibles.

**Le vrai concurrent n'est pas « rester au maximum », c'est le *race-to-sleep*.**
Monter à 32 MHz, finir au plus vite, tomber en mode Stop à quelques µA. Comme
l'énergie dynamique est indépendante de f, courir vite ne coûte rien de plus et
écourte la fenêtre où l'on paie les coûts fixes. C'est presque toujours au moins
aussi bon, et beaucoup plus simple.

**Le DVFS ne gagne que quand on ne peut pas dormir :** trous d'inactivité plus
courts que le coût de réveil (sortie de Stop plus relock de la PLL, quelques
dizaines de µs — à comparer aux périodes de 820 µs et 1,64 ms de nos exemples),
contrainte de latence interdisant le sommeil profond, ou périphérique exigeant
le domaine d'horloge cœur. À noter que la charge de 90 % de l'exemple ne laisse
de toute façon presque aucun temps mort à exploiter.

**Ce qui reste solide dans ce projet**, indépendamment du gain énergétique :
l'apport n'est pas « baisser la fréquence » mais **savoir quand la baisser sans
rater d'échéance**, ce que calculent DRA, OTE et DM_SLACK à partir des WCET
déclarés. C'est une contribution d'ordonnancement, qui tient même si le gain
mesuré s'avère modeste.

### Le banc de mesure existe déjà

`Escapement/CORTEX-Mx/STM32/Examples/stm32l-discovery/IccMeasure.c` enchaîne les
trois paliers et lit le courant via la mesure Icc intégrée à la carte
STM32L-Discovery :

```c
OSSetProcessorSpeed(OS_32MHZ_SPEED);   ...
OSSetProcessorSpeed(OS_16MHZ_SPEED);   ...
OSSetProcessorSpeed(OS_4MHZ_SPEED);    ...
```

Les auteurs d'origine avaient monté exactement l'expérience qu'il faut. Elle n'a
pas été rejouée ici : **tant qu'elle ne l'est pas, tout ce qui précède reste du
raisonnement, pas de la mesure.** Une carte Discovery tranche définitivement, et
le résultat a sa place dans ce README quel qu'il soit — y compris s'il est
décevant.

### Vers quel MCU porter ensuite ?

Le critère de sélection **inverse le classement habituel** : le DVFS paie là où
le sommeil est mauvais. Les meilleurs MCU basse consommation sont précisément
ceux où il sert le moins, puisque le *race-to-sleep* y écrase tout.

Ce qu'il faut chercher : une plage de tension large **et pilotable finement**,
des coûts fixes faibles devant la commutation donc une **fréquence élevée**, un
**sommeil profond médiocre ou coûteux à réveiller**, et une charge **continue**
qui interdit de dormir.

| Cible | Pourquoi | Réserve |
|---|---|---|
| **RP2040** | Tension cœur réglable en continu sur une plage large, horloge programmable de quelques kHz à 133 MHz, et surtout **sommeil médiocre** : pas de Stop à 1 µA, le mode dormant perd les horloges. Le *race-to-sleep* y est faible, le DVFS récupère une vraie niche. Cortex-M0+, déjà couvert par la couche générique. | Flash externe en QSPI XIP, qui ne suit pas la tension cœur : un nouveau coût fixe qui mangera une part du gain. |
| **Cortex-M7 (STM32H7…)** | Le plus gros gain en watts absolus : à 400-550 MHz la commutation domine enfin le budget, donc V² s'applique à une part majoritaire. Charges typiques — audio, SDR, contrôle moteur — **continues**, donc impossibles à endormir. | Cœur non couvert par la couche générique, qui s'arrête à M4. |
| **STM32U5** | Portage **le moins cher** : réutilise la couche STM32 existante. Quatre paliers, gravure 40 nm, moins de fuites. | Contresens à éviter : son Stop 2 descend autour du µA, donc le *race-to-sleep* y domine encore plus que sur L1. Gain probablement **moindre**, pas supérieur. |
| ESP32, Ambiq Apollo | — | À écarter : l'ESP32 a déjà son DFS constructeur (`esp_pm`) avec veille automatique ; chez Ambiq la tension est gérée en interne, sans levier utilisateur. |

Coût d'un portage, mesuré sur la base actuelle :

```
Couche générique Cortex-M (M0/M3/M4)    921 lignes   réutilisable telle quelle
Couche specifique fabricant           ~1800 lignes   a reecrire
  dont le pilote DVFS                   157 lignes   la partie facile
```

Le changement de contexte, les atomiques et l'ordonnanceur ne bougent pas. Le
gros du travail est le timer à comparateur, la table de vecteurs et l'UART —
pas la gestion d'énergie.

**Mais l'ordre compte** : rejouer `IccMeasure.c` sur la Discovery L1 vient
avant. Tant qu'il n'y a pas un chiffre mesuré sur la plateforme déjà en main,
choisir la suivante se fait à l'aveugle.

### Ce que l'émulation ne dira jamais

La plateforme L151 ne modélise ni RCC ni PWR : les écritures du pilote sont
visibles mais sans effet sur la vitesse réelle du cœur. **Aucun émulateur ne
validera du DVFS** — il faudrait modéliser l'effet d'un changement de tension
sur la vitesse d'exécution. Ce que l'émulation prouve ici, c'est que le noyau
ordonnance correctement *et* pilote les bons registres ; pas qu'il économise
de l'énergie. Cette mesure-là demandera du matériel.

## Portage RP2040

Le portage Raspberry Pi Pico vit dans `Escapement/CORTEX-Mx/RP2040/` et tient en
**280 lignes** contre 3 543 pour la couche STM32 — cette dernière consacre 1 757
lignes à ses seules tables de vecteurs par dérivé, là où le RP2040 n'a qu'une
puce et 26 interruptions.

| Fichier | Rôle | Lignes |
|---|---|---:|
| `Escapement_Timer.c` | les cinq fonctions attendues par le noyau, sur TIMER | 180 |
| `Escapement_Interrupts.c` | table des vecteurs et dispatch des 26 IRQ | 83 |
| `Escapement_Processor.h` | paliers de fréquence, pour la variante power-aware | 26 |
| `RP2040_SRAM.ld` | script de link | 50 |

### Deux choix de conception

**Le firmware s'exécute depuis la SRAM.** Le RP2040 démarre normalement depuis
une flash QSPI externe, ce qui impose un second étage de boot de 256 octets avec
son CRC. Lier en SRAM l'évite — c'est ce que fait le type de build `no_flash` du
SDK — et sort la flash de la mesure de consommation, ce qui servira la variante
power-aware. L'application pointe `VTOR` sur `0x20000000` avant d'autoriser les
interruptions.

**Le temps est reconstruit modulo 2³⁰.** Le compteur du RP2040 est un 64 bits
libre qui ne déborde jamais en pratique, alors qu'Escapement attend un compteur
qui reboucle à 2³⁰ et signale chaque rebouclage. `ALARM0` porte l'échéance posée
par `_OSSetTimer`, et `ALARM1`, réarmée sur chaque frontière de 2³⁰, joue le rôle
de l'interruption de débordement.

Surtout, **le timer est cadencé par un tick de 1 µs dérivé de `clk_ref`,
indépendant de `clk_sys`** : changer la fréquence cœur ne déplace pas la base de
temps du noyau. Sur STM32, l'horloge du timer suit l'horloge cœur, ce qui
complique le DVFS.

### Exécution sur matériel réel

Le portage a été **validé sur une Raspberry Pi Pico W**, chargé en SRAM par SWD
avec la Debug Probe officielle — pas besoin du bouton BOOTSEL :

```sh
openocd -f interface/cmsis-dap.cfg -c 'adapter speed 5000' -f target/rp2040.cfg \
        -c 'init; reset halt; load_image build/UARTEchoPico.elf; resume 0x20000000; exit'
```

`UARTEchoPico` renvoie chaque octet reçu sur UART0 (GP0 et GP1, 115200 bauds) :

```
envoyé b'escapement'  -> reçu b'escapement'   ECHO OK
envoyé b'RP2040 ok'   -> reçu b'RP2040 ok'    ECHO OK
envoyé b'0123456789'  -> reçu b'0123456789'   ECHO OK
```

Et `TaskLEDPico`, inspecté par SWD pendant qu'il tournait :

```
PC       = 0x20000864     le wfi d'IdleTask
timer    avance de 136 241 µs entre deux lectures
alarm0   armée 1 620 µs devant le compteur
gpio_out = 0x8            GP3 haut, une tâche s'exécutait
```

L'ordonnanceur arme bien ses échéances dans le futur.

> Sur **Pico W**, la LED intégrée n'est pas sur GP25 : elle est pilotée par la
> puce sans-fil CYW43439, et GP25 sert de chip-select à cette puce.
> `TaskLEDPico` la déclare pourtant comme première sortie — sur une Pico W il
> faut regarder GP2 et GP3 à l'oscilloscope, ou passer par l'UART.

### Le temps du noyau doit avoir une origine

Le compteur du RP2040 tourne librement depuis la mise sous tension et n'est
jamais remis à zéro, alors qu'Escapement suppose que son horloge démarre près de
zéro. Sur une carte allumée depuis quelques heures, le compteur dépasse le
milliard de microsecondes : le noyau se croyait instantanément en retard de
toutes ses échéances et tombait sur son garde-fou de surcharge.

`_OSStartTimer` capture donc l'instant de démarrage, et tous les temps du noyau
sont comptés depuis cette origine. **Ce défaut n'était visible que sur
matériel** : le compteur de l'émulateur repart toujours de zéro.

### Le cœur tourne à 125 MHz

`OSInitializeSystemClocks` engage la PLL, parce que le coût par activation du
noyau fixe un plancher à la période atteignable. Les horloges de référence et
des périphériques, elles, **restent sur le quartz** : la première garde le tick
d'une microseconde exact quelle que soit la fréquence du cœur — c'est
précisément ce qui fait de cette puce une bonne cible pour la variante
power-aware — et la seconde laisse l'UART diviser une fréquence connue du pilote.

### Coût d'une ronde d'ordonnancement

Mesuré sur la carte, de l'interruption matérielle du timer jusqu'au réarmement
de l'échéance suivante — ce qui couvre l'interruption, le gestionnaire logiciel,
le transfert des arrivées vers la file des prêts et l'élection de la tâche
suivante. Jeu de quatre tâches : 1, 10, 20 et 60 ms.

| | |
|---|---:|
| Rondes en 10 s | 10 011 |
| Moyenne | **7,0 µs** |
| Maximum | **26 µs** |

Mille ordonnancements par seconde pour **0,7 % du processeur**, soit environ
875 cycles par ronde à 125 MHz. L'instrumentation est dans `Escapement_Timer.c`
sous `ESCAPEMENT_MEASURE_SCHEDULING_COST`, et les compteurs se lisent par SWD.

### Vérifié par un instrument indépendant

Les trois sorties ont été mesurées au fréquencemètre d'un Bus Pirate v4, relié
à `AUX` et à une masse commune :

| Sortie | Tâche | Attendu | Mesuré | Écart |
|---|---|---:|---:|---:|
| `GP4` | 1 ms, bascule à chaque instance | 500,000 Hz | **500,02 Hz** | +40 ppm |
| `GP2` | 20 ms, impulsion | 50,0000 Hz | **50,0014 Hz** | +28 ppm |
| `GP3` | 60 ms, impulsion de durée variable | 16,66667 Hz | **16,66713 Hz** | +28 ppm |

Huit relevés consécutifs sur `GP3` donnent la même valeur au cent-millième près.
L'écart de +28 ppm, **identique sur les deux mesures les plus précises**, n'est
pas du bruit : c'est la tolérance du quartz de la carte par rapport à la
référence de l'instrument, en plein dans les ±30 ppm habituels.

C'est la seule mesure du projet qui ne dépende **ni du noyau, ni de l'émulateur,
ni du débogueur**. Un second appareil confirme les trois périodes
d'ordonnancement, y compris celle de la tâche dont la durée d'exécution varie
d'une instance à l'autre — sa période, elle, ne bouge pas.

### Le timer doit s'arrêter avec le débogueur

Le RP2040 fige son compteur dès qu'un cœur est arrêté par le débogueur. Ce
comportement est **conservé**, et le désactiver a coûté une longue enquête.

Sans lui, l'horloge du noyau continue de courir pendant les arrêts qu'impose le
chargement par SWD — reset, transfert de l'image, relance du second cœur, soit
plusieurs dizaines de millisecondes. Le noyau démarre donc **déjà en retard
d'autant**. Une tâche de 10 ms ou plus l'absorbe ; une tâche de 1 ms naît avec
vingt périodes de retard, et la boucle de rattrapage la fait réarriver avant
qu'elle ait pu s'exécuter : le garde-fou de surcharge se déclenche, à juste
titre.

L'état capturé au déclenchement le disait déjà, avec trois tâches en retard de
18 à 26 ms au même instant — exactement la durée de la séquence de chargement.
Déboguer un noyau temps réel suppose que son horloge s'arrête avec lui. Mettre
`TIMER_DBGPAUSE` à 0 donne le comportement inverse, utile pour mesurer du temps
mural à travers un arrêt.

### Deux pièges rencontrés

**Recharger sans réinitialiser.** Charger un firmware par SWD par-dessus un
autre laisse les périphériques dans l'état où le précédent les avait mis. Le
timer restait armé, son interruption tombait, et `_OSIOHandler` ne trouvait
aucun descripteur : il partait dans son piège, interruptions coupées. D'où le
`reset halt` avant le `load_image`.

**L'amorçage de l'émission.** Sur l'USART d'un STM32, `TXE` reflète un état :
armer l'interruption la déclenche immédiatement. Sur le PL011 du RP2040, elle se
déclenche sur un franchissement de seuil de la FIFO — l'armer alors que la FIFO
est déjà vide ne produit rien. `OSEnqueueUART` doit écrire les premiers octets
lui-même pour amorcer.

### Exécution vérifiée en émulation

Sur un Pico émulé (voir `emulation/renode/RP2040.md`), les trois tâches
périodiques de `TaskLEDPico.c` sur une seconde :

| Sortie | Période | Instances | Ratio mesuré | Ratio théorique |
|---|---:|---:|---:|---:|
| GPIO 25 (LED intégrée) | 10 ms | 100 | 5,88 | 6,00 |
| GPIO 2 | 20 ms | 50 | 2,94 | 3,00 |
| GPIO 3 | 60 ms | 17 | 1,00 | 1,00 |

Le processeur finit dans le `wfi` d'`IdleTask`. Le firmware n'a **pas encore été
flashé sur une vraie carte**.

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
- [x] **Variante power-aware sur Cortex-M** : `stm32l-discovery-pa` ordonnance
      ses trois tâches et reprogramme la PLL, vérifié en CI.
- [ ] **Rejouer `IccMeasure.c` sur une carte STM32L-Discovery** et consigner le
      gain réel des trois paliers. C'est la seule façon de savoir si le DVFS
      vaut mieux que le *race-to-sleep* sur cette famille.
- [ ] Selon le résultat, porter vers une cible où le gain est structurellement
      plus grand — voir « Vers quel MCU porter ensuite ? ».
- [ ] Proposer en amont les deux correctifs du `Timers.STM32_Timer` de Renode.
- [ ] Reconstituer la documentation utilisateur (le manuel et les notes de
      référence d'origine ont été retirés avec le rebranding).
- [ ] MSP430, seulement si le portage est réactivé : reconstituer le générateur
      de configuration qui produisait les en-têtes par dérivé
      (`Escapement_msp430xNNN.h`), absents du dépôt, et un système de build —
      il n'en existe aucun pour cette cible.

## Licence

Voir `LICENSE` et `NOTICE`.
