# Sokoban 3D

Un Sokoban in tre dimensioni scritto in C++ con OpenGL 3.3, realizzato come progetto per il
corso di Computer Graphics. Il giocatore è un mago che deve spingere una chiave dentro un
forziere attraversando un dungeon, evitando trappole e scheletri che lo inseguono.

![gameplay](docs/screenshot.png)
<!-- sostituisci con uno screenshot tuo, oppure togli questa riga -->

---

## Compilazione ed esecuzione

Sviluppato e testato su **macOS (Apple Silicon)**.

Servono gli strumenti da riga di comando di Xcode e due librerie da Homebrew; GLFW, GLEW,
GLAD, GLM, stb_image e miniaudio sono già inclusi in `dependencies/`.

```bash
brew install assimp freetype
make
./sokoban
```

`make run` compila ed avvia in un colpo solo, `make clean` rimuove l'eseguibile.
Il gioco va lanciato **dalla cartella del progetto**: modelli, texture, shader, livelli e
suoni sono cercati con percorsi relativi.

---

## Comandi

| Tasto | Azione |
|---|---|
| `W` `A` `S` `D` | un passo per pressione (il movimento è a caselle, non continuo) |
| `Click sinistro` | lancia una palla di magia verso il cursore (serve il bastone) |
| `I` | apre e chiude l'inventario |
| `1`–`4` | seleziona direttamente uno slot dell'inventario |
| `↑ ↓ ← →` | scorre gli slot quando l'inventario è aperto |
| `E` | usa l'oggetto selezionato |
| `Rotella premuta` + trascina | ruota la camera attorno al tavolo |
| `Shift` + rotella premuta | sposta la camera lateralmente |
| `Rotella` | zoom |
| `H` | rimette la camera nell'inquadratura di partenza |
| `L` | accende/spegne l'illuminazione dinamica |
| `G` | mostra la griglia logica sopra la scena (utile per il debug) |
| `ESC` | torna al menù, e dal menù esce |
| `INVIO` | rigioca il livello dopo una vittoria o una sconfitta |

Nel menù si naviga col mouse: nuova partita, scelta del livello, classifica dei punteggi,
azzeramento dei record, uscita.

---

## Regole del gioco

- **Obiettivo**: spingere la **chiave** dentro il **forziere**. È l'unico modo di vincere.
- Le **casse** si spingono come nel Sokoban classico (una alla volta, solo se dietro c'è spazio
  libero) e servono da ostacolo.
- Si hanno **3 cuori**. Gli **spuntoni** ne tolgono uno ogni volta che ci si passa sopra —
  la trappola non si consuma. Uno **scheletro** che ti raggiunge toglie un cuore e viene
  rispedito alla sua tomba, così non può prosciugarti tutti i cuori in un colpo solo.
  A zero cuori parte l'animazione di morte e la partita finisce.
- Gli scheletri si muovono **solo quando si muove il giocatore**, una casella per ogni passo,
  scegliendo il cammino più breve con una visita in ampiezza sulla griglia.
- Il **bastone** (livelli 3 e 4) si raccoglie e resta per tutto il tentativo: dà la magia sul
  click del mouse. Uno scheletro colpito torna nella sua tomba e ricompare dopo 10 mosse.
- I **bonus** raccolti finiscono nell'inventario e si usano quando serve; la **maledizione**
  invece scatta appena la si tocca, e le **monete** valgono punti subito.

| Oggetto | Effetto |
|---|---|
| Pozione della vista | allarga il raggio visibile per 15 secondi |
| Lanterna | congela gli scheletri per 8 mosse |
| Razione | dimezza la velocità degli scheletri per 10 mosse |
| Elisir della salute | ridà un cuore (non si può usare a vita piena) |
| Maledizione | restringe la visuale per 10 secondi, scatta da sola |
| Monete | +1000 punti |

**Punteggio**: `10000 − 50 × mosse + 800 × cuori rimasti + bonus raccolti`, mai sotto zero.
I record per livello sono salvati in `scores.dat`.

---

## Struttura del progetto

### Codice

| File | Cosa fa |
|---|---|
| `main.cpp` | crea la finestra GLFW, carica GLAD, tiene il ciclo di gioco e smista tastiera e mouse |
| `game.h` / `game.cpp` | il cuore di tutto: stato della partita, input, logica, camera libera, inventario, magia, punteggio, audio e l'intera fase di disegno |
| `game_level.h` / `game_level.cpp` | legge un `.lvl`, trasforma i codici in oggetti e tiene la griglia logica `TileData` |
| `player_object.h` / `player_object.cpp` | il passo del giocatore: cosa è calpestabile, come si spinge una cassa, l'interpolazione fra due caselle |
| `heart_3d.h` / `heart_3d.cpp` | caricatore di modelli 3D con Assimp (`.obj`, `.fbx`, `.glb`), gestione delle texture e **animazione scheletrica** (skinning con 4 pesi per vertice, fino a 32 ossa) |
| `sphere_mesh.h` / `sphere_mesh.cpp` | genera una sfera UV **interamente in codice**, senza modellatore: è la palla di magia |
| `shader.h` / `shader.cpp` | compila, linka e imposta gli uniform dei programmi GLSL |
| `texture.h` / `texture.cpp` | oggetto texture 2D: `glGenTextures`, wrapping, filtering, `glTexImage2D` |
| `resource_manager.h` / `resource_manager.cpp` | tiene shader e texture in due mappe statiche, così si caricano una volta sola |
| `sprite_renderer.h` / `spriteRendered.cpp` | disegna un quad texturato: sfondo, HUD, menù |
| `text_render.h` / `text_render.cpp` | testo a schermo con FreeType, un atlante di glifi per carattere |
| `game_object.h` / `game_object.cpp` | classe base di tutto ciò che sta su una casella |

### Cartelle

| Cartella | Contenuto |
|---|---|
| `shaders/` | i programmi GLSL: `sprite` (2D), `box` (3D con Phong), `skinned` (3D animato), `orb` (la magia), `text_2d` |
| `resources/` | modelli 3D: `.obj` per l'ambientazione, `.fbx` per mago, scheletro e le clip di animazione |
| `textures/` | immagini per sfondo, interfaccia e personaggi |
| `levels/` | i quattro livelli in formato testo |
| `sounds/` | musica di sottofondo e effetti |
| `fonts/` | i caratteri usati dall'interfaccia |
| `dependencies/` | GLFW, GLEW, GLAD, GLM, stb_image, miniaudio |

---

## Formato dei livelli

Un livello è un file di testo di numeri separati da spazi: **10 righe × 17 colonne**, una
casella per numero.

| Codice | Significato | | Codice | Significato |
|---|---|---|---|---|
| `0` | pavimento | | `9` | punto di comparsa di uno scheletro |
| `1` | muro interno (colonna) | | `10` | maledizione |
| `2` | cassa | | `11` | lanterna |
| `3` | forziere (traguardo) | | `12` | razione |
| `4` | posizione di partenza | | `13` | monete |
| `5` | muro di confine | | `14` | posto libero per un oggetto |
| `6` | spuntoni | | `15` | bastone magico |
| `7` | chiave | | `16` | elisir della salute |
| `8` | pozione della vista | | | |

I codici `4`, `9` e `14` sono **segnaposto**: dopo il caricamento la casella torna pavimento e
resta solo l'informazione. A ogni partita gli oggetti e gli scheletri vengono ridistribuiti a
sorte fra le celle candidate, quindi lo stesso livello non si apre mai due volte uguale, ma il
numero di oggetti resta quello del file e i punteggi restano confrontabili.

Per aggiungere un livello: crea il `.lvl`, caricalo in `Game::Init` accanto agli altri e
aggiungi una voce nel selettore del menù.

---

## Note tecniche

Le cose che il progetto mostra, con il posto dove guardare:

- **Due passate di disegno per fotogramma**: prima la scena 2D in proiezione ortografica
  (sfondo, HUD, menù), poi quella 3D in proiezione prospettica, con il test di profondità
  acceso solo per la seconda.
- **Camera libera** stile viewport: orbita, spostamento laterale e zoom attorno al tavolo,
  ricostruiti con `glm::lookAt`. Con i valori a zero riproduce esattamente l'inquadratura fissa
  originale.
- **Mira**: il click viene riportato dallo schermo alla scacchiera con un'intersezione
  raggio–piano sulla matrice della camera vera, quindi la magia va dove punti anche con la
  camera ruotata.
- **Animazione scheletrica**: le clip stanno in file `.fbx` separati e si agganciano alla mesh
  **per nome dell'osso**; il bastone segue la mano perché la sua matrice è quella dell'osso
  `handslot.r` moltiplicata per quella del mago.
- **Mesh procedurale**: la palla di magia (`sphere_mesh.cpp`) non viene da nessun file. Normali
  e coordinate texture nascono dalla parametrizzazione della sfera; il suo fragment shader non
  usa texture ma un termine di Fresnel per accendere i bordi.
- **Texture**: caricamento con stb_image, `glTexParameteri` per wrapping e filtering, mipmap
  generate per i modelli 3D. Lo sfondo usa apposta `GL_NEAREST` per non sfocare i bordi delle
  caselle, i modelli `GL_LINEAR_MIPMAP_LINEAR`.
- **Illuminazione**: Phong (ambiente + diffusa + speculare) con la luce agganciata al giocatore,
  e un raggio di visibilità che gli oggetti fuori portata non superano.
- **Collisioni senza geometria**: non esiste nessun test fra volumi. Tutto è deciso leggendo il
  codice della casella di destinazione nella griglia `TileData`, che è anche il motivo per cui
  il gioco resta prevedibile e facile da correggere.
- **Audio** con miniaudio: la musica è un `ma_sound` in streaming che va in ciclo, gli effetti
  passano da un gruppo separato così i due volumi sono indipendenti (`MUSIC_VOLUME` e
  `EFFECTS_VOLUME` in cima a `game.cpp`).

---

## Crediti

- Modelli e animazioni: [KayKit](https://kaylousberg.itch.io/) — Adventurers, Skeletons e
  Dungeon pack.
- Suoni da [freesound.org](https://freesound.org): *whoosh* di qubodup (60013), *item pickup*
  di TreasureSounds (332629), musica di bertsz (671900).
- Il codice parte dalla struttura del tutorial [LearnOpenGL / Breakout](https://learnopengl.com/In-Practice/2D-Game/Breakout)
  ed è stato riscritto per il gioco a griglia in tre dimensioni.
