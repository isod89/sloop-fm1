<p align="center"><img src="assets/logo/sloop-logo.png" alt="SLOOP" width="360"></p>

# SLOOP 2.6 — démarrage rapide

**SLOOP** transforme le M-VAVE FM-1 en groovebox à jouer en live, pour tous les styles : trois synthés et une batterie de 16 sons sur les touches blanches, 12 moteurs de synthèse (dont la FM à six opérateurs avec les patchs DX7, les modèles physiques et le bruit), 153 sons rangés par famille (basses, claviers, orgues, nappes, leads, plucks, stabs), 37 kits de batterie (808, 909, trap, phonk, house, techno, UK garage, jungle, amapiano, synthwave, chiptune, ambient…) et tes propres kits, tes propres samples, ghost notes et ratchets, verrous de paramètre, décalage des pas et fills, note repeat, accords sur une touche, 27 effets punch-in, et un écran à la teenage engineering qui montre toujours ce que tes mains peuvent faire. Aucun motif d'usine : tout ce que tu entends, tu le joues.

Le guide complet, chaque bouton, combinaison et page, avec un aide-mémoire d'une page (en anglais) : [GUIDE.md](GUIDE.md). Le manuel (en anglais) : [SLOOP.md](SLOOP.md).

**Nouveau dans la 2.6 :** **27 effets punch-in** (11 de plus sur les touches noires, voir FX plus bas) ; un **écran plus lisible** (police Terminus en gras, tout en majuscules) ; repris de **Felucca 1.4 / 1.5** (Leo Kuroshita) : un effet **INSERT** sur chaque piste, la batterie comprise (SOFT, HARD, FOLD, FUZZ, CRUSH, PHASR, FLANG, CHOR, avec MIX sec / traité ; FX → INSERT et INSERT 2), le **LFO 2** (LFO → LFO 2 : **SYNC** calé sur le tempo, de 4 mesures à 1/32, **TRIG** NOTE / FREE, **POL** BI / UNI), l'**ENV SYNC** (ENV DEST → ESYNC : attaque, decay et release en valeurs de note), le filtre **ANALOG** en passe-bas, passe-bande ou passe-haut (EDIT → FILTER, sur une piste ANALOG), la **durée d'une note en pas** (page STEP, KNOB 3 sur une note : LEN) et la **vélocité de chaque pas** (SEQ + pas de synthé + KNOB 2 : VEL), le **MIDI LEARN** (GLO + F#3) et la correction de l'arpège verrouillé qui suit maintenant TRN, ROOT, SCALE et KEYS (#191) ; **TEXTURE**, une nappe de fond qui boucle sous le mix sans prendre de piste (GLO → TEXTURE : VINYL, TAPE, RAIN, WIND, RADIO, ROOM ou un de tes samples, avec LEVEL, TONE et MOVE, une respiration lente sur 1 à 16 mesures ; sauvée avec le projet et chaque section) ; les **mutes de batterie en direct** (EDIT + une touche noire) ; **un canal MIDI au choix pour chaque piste** et tous les réglages MIDI regroupés (GLO → MIDI : CH1 à CH4 ; MIDI 2 : IN, OTHER, OUT, BEND) ; le **Program Change** MIDI (charge le n-ième son de la liste PRESETS, le kit n sur le canal batterie), le délai **1/4D** et les **REV THROW / ECHO THROW** (FX + OCT− ou OCT+ tenus : départs reverb ou écho à fond tant qu'on tient) repris de Felucca 1.5.1 ; la **saisie des pas depuis un clavier MIDI** (page STEP : les notes jouées sur la piste sélectionnée écrivent le pas) ; la **pédale de sustain** (CC 64) et le **pitch bend** (amplitude : GLO → MIDI 2 → BEND, 1 à 12 demi-tons) ; le **MIDI IN sur un seul canal** (GLO → MIDI 2 → IN = CH 1 … CH 16) ; plus de **craquement** en changeant de son pendant que des notes sonnent, ni quand un filtre (piste, batterie FX + KNOB 4, FILT du master) commence à fermer ; une **sortie plus propre** : le limiteur du master anticipe de 0,7 ms, les kicks et les basses fortes ne craquent plus avec MASTER au maximum (surtout au casque), et FREEZE garde le niveau du mix ; dans l'**éditeur web**, une valeur déjà réglée sur une piste puis la même sur une autre (DIV 1/8 sur la batterie, puis sur la piste 1) est bien envoyée ; et l'installateur trouve une FM-1 sous **CLIPPER**. Les projets de la 2.5 se chargent tels quels ; un projet sauvé en 2.6 ne s'ouvre pas en 2.5 (fais une sauvegarde avant la mise à jour).

**Nouveau dans la 2.5 :** deux moteurs de plus, **PHYS** (modèles physiques : guitares, koto, banjo, sitar et tanpura, harpe, cloches, vibraphone, steel pan, violoncelle à l'archet, tabla, conga, timbales…) et **NOISE** (vent, pluie, océan, vinyle, souffle, riser, bitcrush, radio…), et **153 sons** d'usine (77 nouveaux, tous au même volume). Ils ne prennent pas de mémoire en plus : GRAIN, FM6 et PHYS partagent la même place par piste. Le **swing** s'affiche de 0 (droit) à 100 (le plus fort) au lieu de 50–75 % : le swing lui-même ne change pas. La batterie a maintenant un **envoi delay** : GLO → DRUMS → **DLY** (à côté de REV). Sur la page DRUMS, le cadran **level** va de ghost (à gauche) à hard (à droite). Corrigés (le correctif 2.4.1, inclus) : les patchs DX7 importés avec de l'**AMS** ne font plus de bruit ; le **click** s'entend avec tous les kits, même batterie coupée ou une autre piste en solo ; en mode accord, une touche de la page **STEP** écrit l'accord entier. Les projets de la 2.4 se chargent tels quels.

**Nouveau dans la 2.4 :** le moteur **FM6** (FM à six opérateurs, import des patchs DX7 dans l'éditeur web), les **verrous de paramètre** et le **décalage** des pas, les **fills**, la **chaîne de sections**, le séquenceur vers la **sortie MIDI** (GLO → MIDI 2 → OUT = SEQ), des pas de **1/2, 1 ou 2 mesures**, les délais **pointés** (TIME = 1/8D, 1/16D), **KEYS = ALL KEYS** (toutes les touches éclairées), et avec **NOTES** les notes courtes du séquenceur allument enfin leur touche. Corrigés : un START MIDI pendant le décompte de REC démarre maintenant l'enregistrement ; le swing ne décale plus les triolets ; tourner DIV juste après PLAY ne saute plus de pas ; l'éditeur n'écrit plus en flash pendant la lecture. Les projets et sauvegardes de la 2.3 se chargent tels quels.

---

## Installer

1. Double-clique **`INSTALL-SLOOP.bat`** dans le dossier SLOOP : il compile le firmware et ouvre l'installateur sur `http://localhost:8766/webapp/installer/`.
2. Dans **Chrome ou Edge**, branche le FM-1 en USB (câble de données, directement, sans hub).
3. **INSTALL**, autorise le MIDI, attends *Done*. Garde la fenêtre noire ouverte jusque-là.

L'éditeur web : `http://localhost:8766/webapp/editor/` (ou **`OPEN-EDITOR.bat`**). Hors ligne (2.6) : le lien *Download the editor* (sous Connect, ou sur la page de l'installateur) enregistre l'éditeur en un seul fichier ; double-clique-le, il s'ouvre dans Chrome ou Edge sans internet (Chrome demande une fois d'autoriser le MIDI).

## Un beat en soixante secondes

1. **ALGORITHM** sur la piste **4** (orange, batterie). Les touches blanches jouent 16 sons : **F3 kick**, G3 kick 2, A3 snare, B3 clap, **C4 charley**… **PRESETS** choisit le kit : essaie *808* ou *BOOMBAP*.
2. **REC** : *rec ready*. **Joue librement, à ton tempo** — pas de clic, pas de décompte. Garde **OCT−** enfoncé en frappant pour des ghost notes, **OCT+** pour des frappes fortes.
3. **Appuie sur REC sur le « 1 » qui suit ta dernière mesure** : la boucle se ferme, sa durée fixe le tempo, les frappes se calent sur la grille et la boucle joue aussitôt.
4. **REC** pendant la lecture : tu enregistres par-dessus (overdub). Maintiens **ARP** et garde la touche du charley : un roulement en 1/16, enregistré en ratchets.
5. **ALGORITHM** sur la piste **1** (bleue, *808 BOOM*), **REC**, joue une basse. Maintiens **SEL** et appuie sur la tonalité du morceau (ex. ré) ; sur la piste 2, maintiens SEL et tourne **KNOB 1** sur *7TH* : chaque touche blanche joue un accord de la gamme.
6. Maintiens **FX** + une touche (blanche ou noire) : un effet punch-in. Toujours FX enfoncé : **KNOB 2** = DUST (vinyle), **KNOB 3** = DUCK (pompe).
7. Une erreur ? Maintiens **EDIT** et appuie sur **OCT−** : annuler.

## Les couleurs

| Couleur | Piste | Bouton |
| --- | --- | --- |
| **bleu** | 1 · synthé | KNOB 1 |
| **vert** | 2 · synthé | KNOB 2 |
| **jaune** | 3 · synthé | KNOB 3 |
| **orange** | 4 · batterie | KNOB 4 |

Blanc = ce que tu touches. Rouge = enregistrement.

## Tapé ou maintenu : les calques

Chaque bouton de fonction a deux vies. **Tapé** (appuyé puis relâché sans rien toucher d'autre) : ses pages s'ouvrent, comme avant. **Maintenu** : un **calque** — les 16 touches blanches et les KNOB 1–4 changent de rôle, l'écran affiche les 16 touches en tuiles et les boutons en cadrans. Relâché : on rejoue.

**Repères lumineux :** les tuiles sont 4 rangées de 4 (touches 1–4, 5–8, 9–12, 13–16). Tant qu'un calque est maintenu, et sur la piste batterie, la première touche de chaque rangée (1, 5, 9, 13) s'allume faiblement ; ce qui est actif (un effet, un pas, un son) reste allumé à fond.

**Verrouiller un calque :** maintiens son bouton et tape **HOME** : le calque reste ouvert quand tu lâches le bouton, tes deux mains sont libres pour les touches et les potards (*LOCK* à l'écran, le bouton clignote). N'importe quel autre bouton le referme (HOME, son propre bouton, ENV…) ; PLAY, REC et OCT− / OCT+ continuent de marcher dedans.

| Maintenir | Touches | KNOB 1 · 2 · 3 · 4 |
| --- | --- | --- |
| **FX** — *punch* | effet punch-in tant que la touche est tenue | FILTRE · DUST · DUCK · filtre de la piste |
| **EDIT** — *effacer* | efface ce son / cette note du motif | DÉCALER · LONGUEUR ×2 / ½ · TRANSPOSER · — |
| **ARP** — *roll* | note repeat calé sur la grille | VITESSE · — · — · — |
| **SEQ** — *pas* | les pas 1–16 de la page — un pas tenu : OCT+ sa condition de fill, OCT− efface décalage, verrous et condition | SON / NOTE · DIV · SWING · LONGUEUR — un pas tenu : SON / NOTE · NIVEAU · RATCHET · DÉCALAGE (PRESETS : le verrou, ALGORITHM : son paramètre) |
| **SEL** — *tonalité* (le 2e bouton de la rangée du haut, entre FX et ENV ; pas le potard SELECT) | la tonalité du morceau | ACCORD · GAMME · TOUCHES · TRANSPOSER |
| **GLO** — *mix* | 1–4 mute · 5–8 solo · 9 **fill** tant que tenue · 10 fill sur la mesure suivante · 16 tap tempo | volume des pistes 1 · 2 · 3 · 4 |
| **SAVE** — *chanson* | 1–4 joue la section A–D (mesure suivante ; plusieurs tapées SAVE tenu : une chaîne de sections) · 5–8 sauve la boucle dans A–D · 13 boucle / chanson · 14 REC chanson · 16 l'écran SONG | — |

| Commande | Action |
| --- | --- |
| **PLAY** | lecture / arrêt des quatre pistes (marche aussi dans un calque) |
| **REC** | en lecture : enregistre tout de suite / arrête · à l'arrêt : arme (la première note démarre) · prise libre : ferme la boucle |
| **REC maintenu** | efface la piste choisie (un anneau se remplit, ~2 s ; relâche avant et rien ne se passe) |
| **SAVE** | sur TRACKS : l'écran SONG · ailleurs : les pages SAVE |
| **EDIT + OCT− / OCT+** | annuler / rétablir |
| **ALGORITHM** | choisit la piste (sur toutes les pages) |
| **PRESETS** | le son de la piste, ou le kit de batterie |
| **SELECT** | sur HOME (et dans un calque) : le tempo · sur une page : la page d'avant / d'après du même groupe · écran DRUMS : grid / kit |
| **OCT− / OCT+** | synthés : octave (les deux : retour à 0) · batterie, maintenus : ghost / fort |
| **HOME** | l'écran TRACKS · tapé sur l'écran TRACKS : le visualiseur plein écran (SELECT : 12 styles, HOME encore : le ferme) · maintenu : le menu en sections (écran, lumières, audio / USB, système) · tapé pendant qu'un calque est maintenu : le verrouille |

## La batterie : 16 sons sur les touches blanches

| Touche | Son | Touche | Son | Touche | Son | Touche | Son |
| --- | --- | --- | --- | --- | --- | --- | --- |
| F3 | kick | C4 | charley fermé | G4 | snare 2 | D5 | ride |
| G3 | kick 2 | D4 | charley ouvert | A4 | tom grave | E5 | shaker |
| A3 | snare | E4 | charley pédale | B4 | tom aigu | F5 | conga |
| B3 | clap | F4 | rim | C5 | crash | G5 | cloche |

Une touche noire joue le son de la touche blanche à sa gauche (deux doigts sur un son pour les roulements rapides). Chaque frappe a un **niveau** — GHOST, SOFT, NORM (comme jouée), HARD — et un **ratchet** x1–x4 (la frappe répétée dans son pas).

**Programmer aux touches (2.4) :** SEQ tapé sur la piste batterie ouvre la page **grid**. KNOB 1 choisit le son (on l'entend). Les touches blanches sont alors les 16 pas de ce son : appuie pour poser un pas (on l'entend), appuie encore pour l'effacer. Les quatre premières touches noires choisissent la page de pas (1–16, 17–32…). KNOB 2 se déplace de pas en pas et fait entendre ce que le pas contient. SELECT passe de grid à kit (sur kit, les touches jouent les pads).

## Enregistrer

| Quand | REC fait | Ensuite |
| --- | --- | --- |
| **En lecture** | enregistre la piste **tout de suite** | REC à nouveau arrête l'enregistrement, la boucle continue |
| **À l'arrêt, projet avec des notes** | arme (*rec ready*) | **ta première note démarre la boucle et devient le pas 1** |
| **À l'arrêt, projet vide** | arme (*play freely*) | une **prise libre** : la boucle suit ton jeu (ou, en mode *tempo*, comme avec des notes) |

**L'écran REC règle la façon d'enregistrer** (armé, avant la première note) :

- **KNOB 1 — mode** (projet vide seulement) : **free** = prise libre, le tempo suit ton jeu · **tempo** = enregistre au tempo réglé (SELECT).
- **KNOB 2 — length** : la boucle de la piste, **1, 2 ou 4 mesures**.
- **KNOB 3 — start** : **note** = ta première note démarre la boucle · **count** = appuie sur **PLAY** : une mesure de clics (4, 3, 2, 1 à l'écran), puis la boucle démarre et enregistre.

Le mode et le départ restent comme tu les as laissés (réglages de la FM-1). Avec des notes déjà dans le projet, pas de mode : on enregistre toujours au tempo réglé. Pendant le décompte, REC l'annule et PLAY revient à *rec ready*.

**Prise libre :** joue librement ; l'écran montre les secondes et la boucle que ça donnerait (*2 bars · 92 bpm*). **REC sur le « 1 » qui suit ta dernière mesure** : SLOOP choisit 1, 2 ou 4 mesures au tempo le plus proche, cale tes notes et lance la boucle. **PLAY** abandonne la prise.

Les notes vont au pas le plus proche **tel que tu l'as entendu** (la latence de ~12 ms est compensée). La lumière PLAY clignote à chaque temps : un métronome visuel. Clic audible : GLO → GLOBAL → CLICK.

**Swing** façon MPC : de 0 (droit) à 100 (le plus fort) — GLO → GLOBAL → SWING pour tout, SEQ maintenu + KNOB 3 par piste (ils s'additionnent).

## Les calques en détail

**FX — punch.** Les 16 touches blanches = 16 effets (boucles 1/4 à 1/32, stutter, reverse, tape stop, demi-vitesse, filtres, téléphone, bit crush, alias, gate, écho, wobble), tant que la touche est tenue. Les 11 touches noires (2.6) = 11 de plus : KILL LOW / MID / HIGH (coupe-bandes façon DJ), REWIND (le disque tiré en arrière), OCT UP (une octave plus haut), FLANGER, AUTO-PAN, FREEZE (le mix figé dans la reverb tant que la touche est tenue), LOOP 8T (boucle 1/8 triolet), MONO, FADE OUT (fondu à zéro en deux temps). KNOB 1 = filtre DJ (gauche passe-bas, droite passe-haut, centre OFF), KNOB 2 = DUST, KNOB 3 = DUCK, KNOB 4 = le même filtre sur la piste sélectionnée seulement (2.4 : aussi sur la page FX → FILTER, verrouillable sur un pas).

**EDIT — effacer.** EDIT + une touche : ce son (batterie) ou cette note (synthé) quitte le motif — **en lecture**, de chaque pas que la tête de lecture traverse tant que tu tiens la touche (façon MPC : garde le charley une mesure et les charleys de cette mesure disparaissent) ; **à l'arrêt**, de tout le motif. KNOB 1 décale le motif d'un pas, KNOB 2 le double (×2, copie) ou le coupe en deux, KNOB 3 transpose (synthés). **EDIT + OCT−** annule, **EDIT + OCT+** rétablit.

**Mutes de batterie (2.6).** Sur la piste batterie, **EDIT + une touche noire** passe en mode **MUTE** : les touches blanches coupent et rallument leur son dans le séquenceur, en direct, sans rien effacer (un son coupé affiche *OFF* en rouge). Une touche noire à nouveau, ou EDIT relâché, revient à l'effacement. Les sons restent coupés jusqu'à ce que tu les rallumes ; un projet chargé repart avec tous les sons.

**ARP — roll (note repeat).** ARP + une touche tenue : elle se répète sur la grille à la vitesse de KNOB 1 (1/8, 1/16, 1/32, 32T, 1/64), toujours en rythme. En enregistrement, un roll s'écrit en ratchets.

**SEQ — pas (séquenceur pas à pas).** Les 16 touches blanches = les 16 pas de la page ; les quatre premières touches noires (F#3, G#3, A#3, C#4) ou OCT− / OCT+ = pages 1–4.
- Pas vide : appuie, il est posé (batterie : avec le son affiché, le dernier pad frappé ou KNOB 1 ; synthé : avec la dernière note ou le dernier accord joué).
- Pas posé : appuie et relâche, il s'efface (avec son décalage et ses verrous). Tiens-le et tourne un bouton : il est modifié et gardé — KNOB 1 son / note, **KNOB 2 niveau** (sur un pas de synthé, 2.6 : **VEL**, sa vélocité 1–127), **KNOB 3 ratchet**, **KNOB 4 décalage** ; **PRESETS** un verrou de paramètre, **ALGORITHM** quel paramètre. Plusieurs pas tenus se modifient ensemble. **OCT−** avec un pas tenu efface son décalage et ses verrous.
- **Page STEP** (SEQ tapé, pistes synthé) : sur une note, **KNOB 3 = LEN** (2.6), le nombre de pas que dure la note (les pas suivants deviennent des TIE ; plus court, ses derniers TIE redeviennent des REST). Un **clavier MIDI** écrit aussi le pas (2.6), comme les touches : les notes tenues ensemble sur la piste sélectionnée (son canal : GLO → MIDI) ; tout relâché, le curseur avance. Sur le canal d'une autre piste, à l'arrêt, l'écran affiche *MIDI PLAYS TRACK n* ; pareil, une fois par prise, quand tu enregistres (REC) et que ton clavier joue une autre piste que celle qui enregistre.
- Sans pas tenu : KNOB 2 = DIV (1/4 … 1/32, triolets, et depuis la 2.4 **1/2**, **1BAR**, **2BAR** : des pas d'une ou deux mesures, pour les nappes et les changements d'accords lents), KNOB 3 = swing de la piste (sans effet sur les triolets), KNOB 4 = longueur (1–64 pas ; chaque piste boucle sur sa longueur).

**Décalage (micro-timing).** Un pas tenu + KNOB 4 le sort de la grille : −32 … +31 en 1/64 de pas, moins = en avance (il joue avant son temps, dans le pas précédent), plus = en retard. Ses ratchets suivent ; l'enregistrement et le swing restent sur la grille. Sur la batterie, tout le pas se décale, tous les sons. Un pas décalé porte un point dans le coin de sa tuile.

**Verrous de paramètre.** Un pas tenu + **PRESETS** donne à un paramètre du son une autre valeur *pour ce pas seulement* — le paramètre verrouillé est d'abord le dernier bouton de son que tu as tourné sur une page (ENV, LFO, FX, EDIT, VOICE… ; *FLT* d'ENV DEST tant que tu n'en as tourné aucun), et **ALGORITHM** passe aux autres (en boucle) ; la ligne de titre l'affiche : *lock dst 14*, *lock flt --* (pas encore de verrou). Le premier cran de PRESETS crée le verrou à la valeur actuelle de la piste, les suivants le déplacent ; au pas suivant sans verrou, le paramètre revient à sa valeur (les notes qui sonnent encore suivent, façon Elektron). Un bouton tourné sur la page pendant qu'un verrou agit gagne : cette valeur devient la nouvelle base. Plusieurs verrous peuvent tenir sur un pas (un par paramètre), 24 par piste ; le motif, l'arpégiateur, la tonalité et le mode de voix ne se verrouillent pas. Un pas verrouillé porte le même point qu'un pas décalé ; OCT− avec le pas tenu efface les deux (annuler, EDIT + OCT−, concerne les pas : il ne rend pas les décalages ni les verrous). Décalages et verrous sont sauvés avec le projet et visibles dans l'éditeur web (onglet Séquenceur, détail du pas).

**SEL — tonalité et accords.** Une touche = la tonalité du morceau (la fondamentale des trois synthés). KNOB 1 = **ACCORD** de la piste : OFF, TRIAD, 7TH, 9TH (1-3-7-9, le voicing lo-fi / R&B), SUS4, POWER. Avec un accord, **les touches blanches parcourent la gamme à partir de C4** : C4 = l'accord du I, D4 du II, E4 du III… un doigt, un accord, enregistré comme tel. KNOB 2 = gamme (16 gammes), KNOB 3 = touches (OFF chromatique, SNAP arrondi à la gamme, WHITE gamme sur les blanches), KNOB 4 = transposition. Changer de son ne change jamais la tonalité, le motif ni le mix de la piste.

**CHORD+ (2.4, d'après HiChord et minichord) : les touches noires changent l'accord.** Avec un mode accord, tiens une touche noire en jouant une blanche — ou appuie-la pendant que l'accord est tenu, il change sous ton doigt : **F#** majeur ↔ mineur, **G#** ajoute la 7e, **A#** sus4, **C#** ajoute la 9e, **D#** un renversement (les deux octaves de noires ; tiens-en plusieurs pour les combiner ; la 7e et la 9e viennent de la gamme : en do majeur, G4 (l'accord du V, sol) avec G# donne G7, avec F# + G# Gm7). Ce que tu joues est enregistré tel que tu l'entends. Page **SEL 2** : **STRUM** égrène les notes de l'accord comme une guitare grattée (1 à 60 ms par note ; à droite du grave vers l'aigu, à gauche de l'aigu vers le grave), au clavier et sur les pas d'accord joués par le séquenceur ; **VLEAD** ON place chaque accord au plus près du précédent, pour qu'une suite d'accords glisse au lieu de sauter.

**Conditions de fill.** Un pas tenu + **OCT+** fait tourner sa condition : *normal* → **fill seulement** (il ne joue que pendant un fill) → **pas de fill** (il se tait pendant un fill) → normal. Le fill, c'est toi qui l'appelles en jouant : GLO tenu + touche blanche **9** (*fill*) tant que tu la tiens, ou touche **10** (*bar*) : toute la mesure suivante est un fill, puis c'est fini (rappuie avant la mesure pour annuler). Un pas sauté se tait entièrement (ni note, ni MIDI, ni ratchet, ni verrou : le son revient à sa base) ; le motif continue. Sur la batterie, la condition vaut pour tout le pas. La tuile d'un pas « fill seulement » porte un petit **F** en haut à gauche, celle d'un « pas de fill » un **×** (le point du décalage / verrou reste en haut à droite). OCT− avec le pas tenu remet la condition à normal avec le décalage et les verrous ; STOP coupe tout fill. Les conditions sont sauvées avec le projet et visibles dans l'éditeur web.

**GLO — mix.** Touches blanches 1–4 = mute des pistes 1–4, 5–8 = solo, **9 = fill** tant que la touche est tenue, **10 = fill** sur la mesure suivante, la dernière (G5) = **tap tempo**. KNOB 1–4 = volume des pistes 1–4.

## Annuler, effacer, sauvegarder

- **Annuler / rétablir :** EDIT + OCT− / OCT+ (un niveau : le dernier passage d'enregistrement, effacement, piste effacée, modification de pas ou de motif).
- **Effacer une piste :** maintiens REC ; après 0,7 s un anneau se remplit ; tiens encore ~1,3 s. Relâche avant : rien. Annuler la ramène.
- **Sauvegarder :** SAVE + touches 5–8 sauvent la boucle dans la section / le projet A–D (= SLOT 1–4).
- **Sauvegarde automatique :** à l'arrêt, 2,5 s sans toucher (au plus toutes les 20 s), le projet en cours est gardé ; au rallumage, SLOOP revient comme tu l'as laissé.
- **Nouveau projet :** SAVE → TOOLS → NEW (tourner sur GO).

## Le master : DUST, DUCK, FILT

- **DUST** : le mix dans un vieux sampler et sur un vinyle — saturation douce, fréquence d'échantillonnage réduite, moins de bits, passe-bas, et pendant la lecture un léger souffle et des craquements (à l'arrêt, silence).
- **DUCK** : chaque kick fait « pomper » les synthés (sidechain), sur une croche, à tout tempo.
- **FILT** : filtre DJ — à gauche passe-bas, à droite passe-haut, au centre OFF.

Réglables avec FX maintenu (KNOB 1–3) ou GLO → MASTER.

**TEXTURE (2.6)** : GLO → TEXTURE, une nappe de fond qui boucle sous tout le mix, sans prendre de piste. **SRC** : VINYL (souffle, grondement, craquements), TAPE (souffle de bande qui dérive), RAIN (pluie), WIND (vent), RADIO (parasites), ROOM (ambiance de pièce) ou un de tes samples (USR1–4, en boucle) ; **LEVEL** ; **TONE** (à gauche passe-bas, à droite passe-haut) ; **MOVE** : elle respire (volume et brillance). Page TEXTURE 2 : **RATE** (la respiration sur 1 à 16 mesures), **WHEN** (PLAY : avec la lecture, ALWAYS : tout le temps), **DUCK** (le kick la fait pomper). Sauvée avec le projet, et chaque section A–D a la sienne.

## Mode chanson

Une chanson enchaîne 4 sections, **A–D** (chacune garde les 4 pistes : sons, motifs, kit). Elle se construit en jouant :

1. Fais une boucle (le couplet). Maintiens **SAVE** et appuie sur la **5ᵉ touche blanche** (*save A*). Change la boucle (le refrain) et sauve-la dans **B** (6ᵉ touche), un pont dans **C**, une fin dans **D**. Sur une section déjà utilisée, rappuie dans les 3 s pour confirmer.
2. **Jouer les sections en direct :** SAVE + touche blanche **1–4**. En lecture, la section démarre à la mesure suivante, toujours en rythme ; à l'arrêt, elle devient la boucle tout de suite.
   **Chaîne de sections :** garde SAVE tenu et tape d'autres touches de section — *A B B C*, dans l'ordre que tu veux, jusqu'à 8 — puis relâche : la première démarre à la mesure suivante comme d'habitude, puis chaque suivante quand la précédente a joué la longueur de son motif (la piste la plus longue : 32 pas en 1/16 = 2 mesures), en boucle. La ligne de titre affiche *chain A B B C*, la section qui joue est allumée, la suivante encadrée. Une seule touche de section, STOP ou PLAY en mode chanson arrête la chaîne. REC chanson enregistre ce que la chaîne joue.
3. **Enregistrer la chanson en jouant :** SAVE + touche **14** (*rec*) : dès la mesure suivante, chaque section jouée et son nombre de mesures sont écrits dans la chanson. Rappuie (ou STOP) pour finir : *SONG PARTS 5*. Elle se sauvegarde toute seule une fois arrêtée.
4. **La rejouer :** SAVE + touche **13** bascule *loop* / *song* ; en mode song, **PLAY** joue tout le morceau et s'arrête à la fin.

L'écran **SONG** (SAVE tapé sur TRACKS, ou SAVE + touche 16) montre la chaîne et permet de la retoucher aux boutons (KNOB 1 étape, 2 section, 3 mesures, 4 nombre d'étapes).

## FM6 : la FM à six opérateurs et les patchs DX7

**FM6** est le dixième moteur : la synthèse FM classique à six opérateurs, le son du DX7 (pianos électriques, cloches, nappes de verre, basses rondes, cuivres). C'est msfa, le cœur de Dexed, porté en calcul entier par Leo Kuroshita pour Felucca 1.0. Huit patchs d'usine sont dans la banque de sons (TINE EP, GLASS BELL, ROUND BASS, BRASS SECT, SOFT PAD, WOOD BARS, DRAWBARS, NYLON PICK). Sur la FM-1, les huit réglages **EDIT** agissent par-dessus le patch : **ALG** l'algorithme (PAT = celui du patch, 1–32 un autre), **FB** le feedback, **MLVL** la brillance, **MRAT** le ratio des modulateurs, **MEG** leurs enveloppes plus lentes ou plus rapides, **VMOD** la vélocité, **DTUN** l'élargissement, **PTCH** le patch : **F1–F8** ceux d'usine, **B1–B27** ta banque. Les enveloppes du patch font chaque note : l'ADSR de la piste ne sert pas ici.

Dans l'éditeur web, onglet **Sound**, le panneau **FM6** (quand la piste joue FM6) édite tout le patch : **Send to track** (ça joue tout de suite), **Store in bank** (B1–B27, sur la flash de la FM-1 ; arrête la lecture avant), et surtout **Import SysEx** : un fichier DX7 d'une voix ou une banque de 32 voix (choisis-en une) — les milliers de patchs DX7 et Dexed du web jouent sur la FM-1. Tu peux aussi glisser le fichier .syx sur le panneau. **Store cartridge in bank** (2.5) range toute une cartouche d'un coup : les 27 premières voix sont cochées (ou coche celles que tu veux, 27 au plus), et une seule écriture les met dans B1, B2… dans l'ordre ; l'éditeur demande avant de remplacer, le reste de la banque ne bouge pas. Avant d'envoyer, tu peux choisir de **les ajouter aussi aux user presets** (dans les slots libres, ou à partir d'un slot) : chaque voix devient un preset à son nom, et sur la FM-1 tu la choisis par son nom avec **PRESETS** ou **SELECT**, comme n'importe quel son. Sur la FM-1, choisis-les avec **PTCH** (B1–B27 après F1–F8). **Export SysEx** les réécrit pour Dexed ou un vrai DX7. Un projet garde le PTCH de la piste, pas le patch lui-même : range un patch envoyé dans la banque pour le retrouver avec le projet.

## L'éditeur web

Chrome ou Edge, FM-1 en USB, **Connect**. Il suit l'appareil en direct. Depuis la 2.4, il a l'allure de la FM-1 : écran noir, les quatre couleurs des pistes, sa police pixel. Les pages sont dans la barre de gauche (**Track** : Sound, Sequencer, Tracks ; **Sounds** : Library, Samples, Drum kit ; **Device** : Projects, Settings), les quatre pistes en haut (clique pour en éditer une). Les réglages sont rangés comme sur la machine : une carte par bouton (ENV, LFO, EDIT…), ses pages en rangées de quatre boutons colorés ; tire une valeur vers le haut ou le bas comme un bouton (Maj : fin).

- **Sequencer** sur la piste batterie : une grille 16 sons × pas, avec le **kit**. Choisis un **niveau** (GHOST, SOFT, NORM, HARD) et un **roll** (x1–x4), puis clique : une frappe ; reclique (même niveau et roll) : effacée ; Maj+clic : un niveau plus fort.
- **Drum kit** (2.4) : **ta propre batterie**. 16 pads, un par son de la piste batterie (KICK, KICK 2, SNARE, CLAP, HAT, OPEN HAT, PEDAL, RIM, SNARE 2, LOW TOM, HI TOM, CRASH, RIDE, SHAKER, CONGA, COWBELL). Dépose un WAV sur un pad, ou choisis-en plusieurs : ils sont rangés d'après leur nom (*kick*, *bd*, *snare*, *hh*, *open*, *crash*…). Par pad : **Pitch** (±12 demi-tons), **Gain**, **Length** ; clique sur l'onde pour écouter. Puis **où va le kit** : **USR3+4**, le gros kit (environ 15 s, l'éditeur le répartit tout seul sur USR3 et USR4 ; USR1 et USR2 restent libres pour tes instruments), ou un seul slot, **USR1** à **USR4** (environ 7,4 s). Chaque choix montre ce qu'il contient et ce qui sera remplacé. Le compteur montre le temps utilisé ; trop long : raccourcis, ou **Fit the slot**. Donne un nom, **Send**, puis **Use … on the drum track** (ou KIT = USR3+4, le dernier de la liste). Tout marche comme avec les autres kits : pas, niveaux, ratchets, fills, la grille, MIDI canal 10, le hat fermé coupe l'ouvert. Un slot est soit un kit, soit un instrument (SAMPLE) : par exemple USR1 et USR2 pour tes instruments, et un gros kit sur USR3+4. Le slot **USR4** est nouveau dans la 2.4 (de la flash qui ne servait à rien). **Save as ZIP** / **Open ZIP** pour partager un kit.
- **Drum synth** (2.5) : **tes propres kits synthétisés**, SYN1 à SYN4 (les derniers du KIT de la batterie). Choisis un kit (au départ : copies de 808, 909, TRAP, TECHNO ; **Start from** + **Copy** pour partir d'un des 32 kits), donne-lui un nom (l'écran DRUMS l'affiche), choisis un son (KICK, SNARE, CLAP…) et change tout : la forme d'onde et la chute de hauteur, le HOLD et le DECAY, la 2e harmonique, le CLICK, le bruit (WHITE, METAL du 808, CHIP, CLAP), le filtre, le DRIVE et le niveau ; le FM-1 joue chaque changement tout de suite. Double-clic sur une valeur : celle du kit de départ. **Store on the FM-1** les garde en mémoire (song arrêtée), **Use on the drum track** choisit le kit, **Save file / Open file** pour partager. Les sauvegardes les gardent aussi.
- **Settings → MASTER** : DUST, DUCK, FILT, ROLL. **Tracks** : les quatre tranches (volume, pan, mute ; SOLO et REC affichés). **Samples** (refaite en 3 étapes) : ① choisis un slot (USR1 à USR4, chacun montre ce qu'il contient) ; ② fais le son, **depuis des fichiers** (un WAV par note, pour un instrument : le nom donne la note, `PIANO_C4.wav`, un clavier montre quelles touches jouent quel fichier) ou **en découpant un enregistrement** (CHOP) ; ③ **Send to USRn**, puis **Play this slot on track 1/2/3** règle la piste en SAMPLE avec SET = USRn. Le découpage CHOP (même un enregistrement de plus de 7 s : coche les chops à garder, raccourcis-les, ou **Fit to slot**).

## Clavier MIDI

- **Prise MIDI IN** (jack 3,5 mm TRS du FM-1) : un clavier ou des pads avec une sortie MIDI, via un adaptateur TRS ↔ DIN MIDI. Si rien ne joue, essaie l'autre type d'adaptateur (type A / type B).
- **USB** : depuis un ordinateur ou un téléphone (DAW, appli de routage MIDI) ou un boîtier « USB MIDI host ». Un clavier USB branché directement sur le FM-1 ne peut pas marcher : ce sont deux appareils USB, il faut un hôte.
- **Canaux :** 1, 2, 3 = pistes synth 1, 2, 3 · 10 = la batterie · 4 à 16 = **la piste sélectionnée** (règle ton clavier sur le canal 4 et il suit ALGORITHM).
- **Tes propres canaux (2.6) :** GLO → **MIDI** règle le canal de chaque piste (**CH1**, **CH2**, **CH3**, **CH4** la batterie : OFF ou 1–16, en entrée et en sortie ; deux pistes ne partagent jamais un canal, le bouton saute ceux déjà pris et dit à qui ils sont). GLO → **MIDI 2** : **IN**, **OTHER** (ce que font les canaux qu'aucune piste n'a : **SEL** la piste sélectionnée, comme avant ; **OFF** ignorés), **OUT**, **BEND**. Notes, CC et sortie MIDI suivent : un séquenceur externe joue la batterie sur 10, la basse sur 2, la nappe sur 11 et le lead sur 8. Réglages de la FM-1, gardés à l'extinction (le canal batterie aussi, désormais : un projet ne le change plus).
- **Horloge MIDI :** GLO → SYSTEM → **SYNC** = **USB** ou **TRS**. SLOOP suit le tempo, START, CONTINUE et STOP du maître, sans jamais dériver. Sans horloge pendant une demi-seconde, PLAY rejoue au tempo du FM-1. SYNC est un réglage de la FM-1 : il reste quand tu charges un projet.
- **Horloge seule :** GLO → SYSTEM → **IN** = **CLOCK** : SLOOP suit l'horloge (et START / STOP) mais ignore les notes qui arrivent, pratique quand la DAW envoie aussi des notes à d'autres appareils. **NOTES** (par défaut) : notes et horloge. Un réglage de la FM-1, comme SYNC.
- **Sortie MIDI :** ce que tu joues aux touches sort toujours en MIDI USB (chaque piste sur son canal, la batterie sur 10). GLO → MIDI 2 → **OUT** = **SEQ** envoie aussi ce que jouent le séquenceur, l'arpégiateur et les rolls : pilote un autre synthé, ou enregistre le motif en notes dans ta DAW. Les notes venues d'un ordinateur ou du jack ne sont jamais renvoyées (pas de boucle MIDI). C'est un réglage de la FM-1, comme SYNC.
- **Les CC MIDI (2.5)** : les potards d'un contrôleur MIDI règlent le son de la piste que joue leur canal (comme les notes) : CC 7 volume, 10 panoramique, 74 le FILTRE de la piste (64 = coupé, en dessous passe-bas, au-dessus passe-haut), 71 résonance (ANALOG, TRIO, VOICE, NOISE), 73 / 75 / 72 attaque / decay / release, 5 glide, 91 / 93 / 94 les envois reverb / chorus / delay. Sur le canal de la batterie : 7, 91 et 94 = GLO → DRUMS → LVL, REV et DLY. Avec IN = CLOCK, les CC sont ignorés. La carte vient de Felucca 1.1.5.
- **Sustain et pitch bend (2.6)** : la pédale de sustain (CC 64) garde les notes relâchées tant qu'elle est enfoncée (enregistrées comme entendues) ; le pitch bend fait glisser la hauteur de la piste du canal, jusqu'à BEND demi-tons (GLO → MIDI 2 : 1 à 12, 2 par défaut).
- **Un seul canal (2.6) :** GLO → SYSTEM → **IN** = **CH 1 … CH 16** : SLOOP ne répond qu'à ce canal, sur la piste sélectionnée (notes et CC), pour un séquenceur dont les autres canaux pilotent d'autres appareils.
- **MIDI LEARN (2.6, d'après Felucca 1.5) :** **GLO + F#3** (la ligne du haut affiche *LEARN*), tourne le bouton de SLOOP voulu (*T1 CUT: SEND A CC*), puis le potard de ton contrôleur (*CC 21 = T1 CUT*) ; d'autres si tu veux ; **GLO + F#3** pour finir. Un CC appris règle ce paramètre de cette piste sur n'importe quel canal, à la place de la carte ci-dessus (16 au plus ; gardés à l'extinction). Menu (tiens HOME) → SYSTEM → **MIDI LEARN CLEAR**, OCT+ : tout est oublié.
- Le MIDI Bluetooth n'est pas pris en charge (la radio reste éteinte).

## Le visualiseur

Sur l'écran TRACKS, tape **HOME** : tout l'écran devient un visualiseur de ce que SLOOP joue. **SELECT** passe d'un style à l'autre (le nom s'affiche une seconde ; le dernier choisi est gardé) : oscilloscope, spectre, spectrogramme, Lissajous, VU-mètres, cercle, tape, LCD, bounce, orbit, wires et le logo SLOOP vivant (les quatre bandes de la voile sont les pistes, le cadran le niveau du mix, le bateau tangue sur l'onde). **HOME** encore, ou n'importe quel bouton de page, le ferme. Les touches, PLAY, REC et les calques marchent comme d'habitude ; KNOB 1 à 4 ne font rien pendant ce temps, et le tempo se règle avec GLO + SELECT. Le visualiseur voit le son comme si **MASTER** était au maximum : il ne suit pas le volume, et même avec MASTER à 0 il bouge comme à plein volume. Ça ne coûte rien au son.

## Lumières (jouer dans le noir)

Maintiens **HOME** pour le menu. Il est rangé en quatre sections, comme les pages : **SCREEN** (COLOR, ZOOM), **LIGHTS** (LIGHTS, KEYS, NOTES), **AUDIO** (LOWCUT, USB AUDIO, USB SERIAL), **SYSTEM** (calibration, à propos). **SELECT** passe d'une section à l'autre, **KNOB 1, 2, 3** règlent directement les lignes de la section (chaque ligne a la couleur de son bouton), PRESETS déplace le curseur, OCT+ fait défiler le réglage du curseur ou l'ouvre, OCT− ferme. C'est sauvegardé avec les réglages de la FM-1, pas avec un projet : charger un projet ou NEW PROJECT n'y change rien.

- **LIGHTS** — OFF, LOW, MID, HIGH : tous les boutons s'éclairent à ce niveau, on lit les étiquettes dans le noir (illisibles sur un FM-1 noir quand ils sont éteints). Ce qui est actif (la page, PLAY, REC, l'octave) reste en pleine lumière et clignote comme avant.
- **KEYS** — OFF, C KEYS, WHITE KEYS, ALL KEYS : les touches Do, toutes les touches blanches, ou toutes les touches s'éclairent aussi.
- **NOTES** — ON : sur la piste synth sélectionnée, les notes qui sonnent allument leur touche, jouées au clavier ou par le séquenceur (par @renebohne). Depuis la 2.4, chaque note reste allumée au moins un dixième de seconde, comme un coup de batterie : les notes courtes du séquenceur se voient aussi. Ça marche sur toutes les pages et dans tous les calques : là où les touches jouent ou effacent des notes (EDIT, ARP, SAVE, SEL), les notes sont allumées ; là où les touches sont des tuiles (effets FX, pas SEQ, mute / solo GLO), les notes brillent faiblement et les tuiles gardent leur pleine lumière.

L'éclairage faible est une impulsion très courte à chaque balayage du panneau : pas de scintillement.

## Enregistrer la FM-1 sur l'ordinateur (audio USB)

Branchée en USB, la FM-1 est aussi une **entrée audio** nommée **Felucca** (44,1 ou 48 kHz, stéréo, sans pilote ; le 48 kHz depuis la 2.5, pour les téléphones et les applis qui ne prennent que ça). Dans ta DAW, dans Audacity ou dans une appli d'échantillonnage sur ton téléphone (câble ou adaptateur USB-C), choisis cette entrée et enregistre : tu as la sortie master, exactement ce qu'on entend au casque (avec DUST, DUCK et FILT). **Le niveau : menu HOME → USB AUDIO.** **MASTER** (par défaut) : l'enregistrement suit le bouton MASTER, comme le casque. **FULL** : niveau fixe, comme MASTER à fond, protégé de la saturation par le limiteur, quel que soit le bouton ; MASTER ne règle alors que le casque (le bon choix pour une carte son sans réglage de niveau). Le MIDI, l'éditeur et l'installateur marchent toujours sur le même câble. La première fois, l'ordinateur reconfigure l'appareil (MIDI + audio) ; le port MIDI garde son nom. Cette entrée vient de Felucca 1.0. **Sur Mac (macOS 13 à 15)**, laisse menu HOME → **USB SERIAL** sur OFF (par défaut) : avec la console série, ces Mac ne montrent pas l'entrée audio (un changement s'applique au prochain démarrage).

## Sauvegarde complète

Dans l'éditeur, onglet **Projects** → **Backup** : **Save a backup** enregistre tout le contenu de la FM-1 dans un seul fichier (le morceau en cours, les projets 1 à 4, les presets utilisateur, la banque FM6, les samples USR1 à USR4, les réglages). **Restore from a file** remet tout comme dans le fichier (ce qui est sur la FM-1 est remplacé). Arrête la lecture (PLAY) avant de restaurer.

## Secours

- **Secours USB :** maintiens **OCT−** seul à l'allumage (*SLOOP USB RESCUE*), puis réinstalle.
- **Installation interrompue :** le FM-1 reste en mode mise à jour ; relance INSTALL et il termine. Un paquet abîmé est refusé et le FM-1 attend un paquet correct.
- **Retour au firmware officiel :** sur la page d'installation, ouvre **Return to the official firmware (V15)** : fais d'abord une sauvegarde avec l'éditeur, télécharge FM-1 V15 sur m-vave.com, choisis son fichier FM-1.fwsc (seul ce fichier exact est accepté) et installe-le. M-UPGRADE de M-VAVE marche aussi (ferme les autres applis qui utilisent le MIDI).

SLOOP est libre (GPL-3.0), basé sur Felucca de Leo Kuroshita (Hügelton Instruments). M-VAVE et FM-1 sont des marques de leurs propriétaires ; SLOOP n'y est pas affilié.
