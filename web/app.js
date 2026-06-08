const VERSIONS = [
  { id: 30, label: "v30 time mgmt" },
  { id: 29, label: "v29 lazy SMP" },
  { id: 28, label: "v28 LMP" },
  { id: 27, label: "v27 PVS" },
  { id: 26, label: "v26 LMR" },
  { id: 25, label: "v25 opening book" },
  { id: 24, label: "v24 hanging pieces" },
  { id: 23, label: "v23 space" },
  { id: 22, label: "v22 extended pawn" },
  { id: 21, label: "v21 passed pawns" },
  { id: 20, label: "v20 persistent TT" },
  { id: 19, label: "v19 killer/history" },
  { id: 18, label: "v18 null move" },
  { id: 17, label: "v17 delta qsearch" },
  { id: 16, label: "v16 SEE qsearch" },
  { id: 15, label: "v15 piece placement" },
  { id: 14, label: "v14 repetition draw" },
  { id: 13, label: "v13 mop-up eval" },
  { id: 12, label: "v12 check extension" },
  { id: 11, label: "v11 TT hash move" },
  { id: 10, label: "v10 transposition table" },
  { id: 9, label: "v9 rook placement" },
  { id: 8, label: "v8 knight outposts" },
  { id: 7, label: "v7 phased eval" },
  { id: 6, label: "v6 pawn/king eval" },
  { id: 5, label: "v5 move ordering" },
];

const game = new Chess();
let board = null;
let boardOrientation = "white";
let autoPlayTimer = null;
let engineBusy = false;
let engineStepPending = false;
let pendingPromotion = null;
let selectedSquare = null;

let fenHistory = [];
let moveMeta = [];
let viewIndex = 0;
let clearTtOnNextSearch = false;
let stockfishBestMove = null;

const modeEl = document.getElementById("mode");
const whiteVersionEl = document.getElementById("white-version");
const blackVersionEl = document.getElementById("black-version");
const depthEl = document.getElementById("depth");
const movetimeEl = document.getElementById("movetime");
const delayEl = document.getElementById("delay");
const statusEl = document.getElementById("status");
const promotionModal = document.getElementById("promotion-modal");
const gameOverEl = document.getElementById("game-over");
const gameOverTitleEl = document.getElementById("game-over-title");
const gameOverSubtitleEl = document.getElementById("game-over-subtitle");
const startFenEl = document.getElementById("start-fen");
const sfEvalTextEl = document.getElementById("sf-eval-text");
const sfEvalBarEl = document.getElementById("sf-eval-bar");
const whiteCapturedEl = document.getElementById("white-captured");
const blackCapturedEl = document.getElementById("black-captured");
const whiteAdvEl = document.getElementById("white-adv");
const blackAdvEl = document.getElementById("black-adv");

const MATERIAL_BASE_COUNTS = { p: 8, n: 2, b: 2, r: 2, q: 1 };
const MATERIAL_VALUES = { p: 1, n: 3, b: 3, r: 5, q: 9 };
const PIECE_ORDER = ["q", "r", "b", "n", "p"];
const BLACK_GLYPHS = { p: "♟", n: "♞", b: "♝", r: "♜", q: "♛" };
const WHITE_GLYPHS = { p: "♙", n: "♘", b: "♗", r: "♖", q: "♕" };

function fillVersionSelect(select, defaultVersion) {
  select.innerHTML = "";
  for (const v of VERSIONS) {
    const option = document.createElement("option");
    option.value = String(v.id);
    option.textContent = v.label;
    select.appendChild(option);
  }
  select.value = String(defaultVersion);
}

function describeGameResult() {
  if (!game.game_over()) {
    return null;
  }

  if (game.in_checkmate()) {
    const loser = game.turn();
    const winner = loser === "w" ? "b" : "w";
    const human = humanColor();
    const mode = currentMode();

    if (mode === "engine") {
      return {
        title: "Checkmate",
        subtitle: `${winner === "w" ? "White" : "Black"} wins`,
        type: "checkmate",
        status: `Checkmate. ${winner === "w" ? "White" : "Black"} wins.`,
      };
    }

    if (human === winner) {
      return {
        title: "Checkmate",
        subtitle: "You win!",
        type: "win",
        status: "Checkmate. You win!",
      };
    }

    return {
      title: "Checkmate",
      subtitle: "You lose.",
      type: "loss",
      status: "Checkmate. You lose.",
    };
  }

  if (game.in_stalemate()) {
    return {
      title: "Stalemate",
      subtitle: "Draw.",
      type: "draw",
      status: "Stalemate. Draw.",
    };
  }

  return {
    title: "Game over",
    subtitle: "Draw.",
    type: "draw",
    status: "Game over. Draw.",
  };
}

function showGameOver(result) {
  gameOverEl.className = `game-over ${result.type}`;
  gameOverTitleEl.textContent = result.title;
  gameOverSubtitleEl.textContent = result.subtitle;
  gameOverEl.classList.remove("hidden");
}

function hideGameOver() {
  gameOverEl.classList.add("hidden");
}

function updateGameOverBanner() {
  if (!isLivePosition()) {
    hideGameOver();
    return;
  }
  const result = describeGameResult();
  if (result) {
    showGameOver(result);
  } else {
    hideGameOver();
  }
}

function setStatus(text) {
  statusEl.textContent = text;
}

function updateStockfishDisplay(result) {
  if (!sfEvalTextEl || !sfEvalBarEl) {
    return;
  }
  sfEvalTextEl.textContent = result.text;
  sfEvalTextEl.classList.toggle("thinking", Boolean(result.thinking));
  if (result.barWidth != null) {
    sfEvalBarEl.style.width = `${result.barWidth}%`;
  }
  if (result.thinking) {
    stockfishBestMove = null;
  } else {
    stockfishBestMove = result.bestMove || null;
  }
  applyBoardHighlights();
}

function refreshStockfishEval() {
  if (typeof requestStockfishEval !== "function") {
    return;
  }
  requestStockfishEval(fenHistory[viewIndex] || game.fen());
}

function materialCounts() {
  const counts = {
    w: { p: 0, n: 0, b: 0, r: 0, q: 0 },
    b: { p: 0, n: 0, b: 0, r: 0, q: 0 },
  };
  const rows = game.board();
  for (const row of rows) {
    for (const piece of row) {
      if (!piece || piece.type === "k") continue;
      counts[piece.color][piece.type]++;
    }
  }
  return counts;
}

function capturedPiecesFor(capturedColor, counts) {
  const out = { p: 0, n: 0, b: 0, r: 0, q: 0 };
  for (const type of PIECE_ORDER) {
    out[type] = Math.max(0, MATERIAL_BASE_COUNTS[type] - counts[capturedColor][type]);
  }
  return out;
}

function materialFromCaptured(captured) {
  let total = 0;
  for (const type of PIECE_ORDER) {
    total += captured[type] * MATERIAL_VALUES[type];
  }
  return total;
}

function capturedString(captured, glyphs) {
  let text = "";
  for (const type of PIECE_ORDER) {
    text += glyphs[type].repeat(captured[type]);
  }
  return text;
}

function updateMaterialBalance() {
  if (!whiteCapturedEl || !blackCapturedEl || !whiteAdvEl || !blackAdvEl) {
    return;
  }
  const counts = materialCounts();
  const capturedByWhite = capturedPiecesFor("b", counts);
  const capturedByBlack = capturedPiecesFor("w", counts);
  const whiteMaterialGain = materialFromCaptured(capturedByWhite);
  const blackMaterialGain = materialFromCaptured(capturedByBlack);
  const diff = whiteMaterialGain - blackMaterialGain;

  whiteCapturedEl.textContent = capturedString(capturedByWhite, BLACK_GLYPHS);
  blackCapturedEl.textContent = capturedString(capturedByBlack, WHITE_GLYPHS);

  whiteAdvEl.textContent = diff > 0 ? `+${diff}` : "";
  blackAdvEl.textContent = diff < 0 ? `+${-diff}` : "";
  whiteAdvEl.classList.toggle("positive", diff > 0);
  blackAdvEl.classList.toggle("positive", diff < 0);
}

function squareClass(square) {
  return `square-${square.charAt(0)}${square.charAt(1)}`;
}

function clearHighlights() {
  $("#board [class*='square-']").removeClass(
    "highlight-selected highlight-legal highlight-capture highlight-last-from highlight-last-to highlight-sf-from highlight-sf-to"
  );
}

function applyBoardHighlights() {
  clearHighlights();

  if (viewIndex > 0) {
    const last = moveMeta[viewIndex - 1];
    if (last) {
      highlightSquare(last.from, "highlight-last-from");
      highlightSquare(last.to, "highlight-last-to");
    }
  }

  if (stockfishBestMove) {
    highlightSquare(stockfishBestMove.from, "highlight-sf-from");
    highlightSquare(stockfishBestMove.to, "highlight-sf-to");
  }
}

function highlightSquare(square, className) {
  $(`#board .${squareClass(square)}`).addClass(className);
}

function isLivePosition() {
  return viewIndex === fenHistory.length - 1;
}

function canInteract() {
  return isLivePosition() && !engineBusy && !game.game_over();
}

function searchConfig(side) {
  const version = Number(side === "w" ? whiteVersionEl.value : blackVersionEl.value);
  const config = {
    fen: game.fen(),
    repetition_fens: fenHistory.slice(0, viewIndex + 1),
    depth: Number(depthEl.value),
    movetime_ms: Number(movetimeEl.value),
    version,
    use_book: version >= 25,
  };
  if (clearTtOnNextSearch) {
    config.clear_tt = true;
    clearTtOnNextSearch = false;
  }
  return config;
}

async function requestEngineMove(side) {
  const response = await fetch("/api/search", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(searchConfig(side)),
  });
  const data = await response.json();
  if (!data.ok) {
    throw new Error(data.error || "search failed");
  }
  return data;
}

function currentMode() {
  return modeEl.value;
}

function humanColor() {
  const mode = currentMode();
  if (mode === "human-white") return "w";
  if (mode === "human-black") return "b";
  return null;
}

function isHumanTurn() {
  const color = humanColor();
  return color !== null && game.turn() === color;
}

function isEngineTurn() {
  const mode = currentMode();
  if (mode === "engine") return !game.game_over();
  const color = humanColor();
  return color !== null && game.turn() !== color && !game.game_over();
}

function applyLastMoveHighlight() {
  applyBoardHighlights();
}

function showLegalMoves(square) {
  clearHighlights();
  highlightSquare(square, "highlight-selected");
  const moves = game.moves({ square, verbose: true });
  for (const move of moves) {
    const targetPiece = game.get(move.to);
    const isCapture = Boolean(targetPiece) || move.flags.includes("e") || move.flags.includes("c");
    highlightSquare(move.to, isCapture ? "highlight-capture" : "highlight-legal");
  }
  if (stockfishBestMove) {
    highlightSquare(stockfishBestMove.from, "highlight-sf-from");
    highlightSquare(stockfishBestMove.to, "highlight-sf-to");
  }
}

function updateViewStatus(extra = "") {
  const totalPlies = fenHistory.length - 1;
  if (!isLivePosition()) {
    const move = viewIndex > 0 ? moveMeta[viewIndex - 1] : null;
    const label = move ? `${move.san}${move.version ? ` (v${move.version})` : ""}` : "Start";
    setStatus(`Reviewing ply ${viewIndex}/${totalPlies}: ${label}. ← → to step.${extra ? " " + extra : ""}`);
    return;
  }
  if (extra) {
    setStatus(extra);
  }
}

function syncBoard() {
  board.position(fenHistory[viewIndex], false);
  applyLastMoveHighlight();
  updateMaterialBalance();
  updateGameOverBanner();
  refreshStockfishEval();
  if (!canInteract() || !isHumanTurn()) {
    selectedSquare = null;
  }
}

function initHistory() {
  fenHistory = [game.fen()];
  moveMeta = [];
  viewIndex = 0;
  selectedSquare = null;
  stockfishBestMove = null;
  clearHighlights();
}

function appendToHistory(move, meta = {}) {
  fenHistory.push(game.fen());
  moveMeta.push({
    from: move.from,
    to: move.to,
    san: move.san,
    ...meta,
  });
  viewIndex = fenHistory.length - 1;
  selectedSquare = null;
  syncBoard();
}

function stepHistory(delta) {
  if (fenHistory.length <= 1) {
    return;
  }
  const next = Math.max(0, Math.min(fenHistory.length - 1, viewIndex + delta));
  if (next === viewIndex) {
    return;
  }
  viewIndex = next;
  selectedSquare = null;
  syncBoard();
  updateViewStatus();
  document.getElementById("engine-move").disabled = !isEngineTurn() || !isLivePosition();
}

function finishGame(resultText) {
  stopAutoPlay();
  const described = describeGameResult();
  setStatus(described ? described.status : resultText);
  selectedSquare = null;
  clearHighlights();
  applyLastMoveHighlight();
  updateGameOverBanner();
}

function afterHumanMove(move) {
  appendToHistory(move);
  if (game.game_over()) {
    if (game.in_checkmate()) finishGame("Checkmate.");
    else if (game.in_stalemate()) finishGame("Stalemate.");
    else finishGame("Draw.");
    return;
  }
  updateViewStatus(`${move.san} | your move`);
  applyEngineMove();
}

function choosePromotion(source, target) {
  return new Promise((resolve) => {
    pendingPromotion = { source, target, resolve };
    promotionModal.classList.remove("hidden");
  });
}

function completePromotion(piece) {
  if (!pendingPromotion) return;
  const { source, target, resolve } = pendingPromotion;
  pendingPromotion = null;
  promotionModal.classList.add("hidden");
  resolve(game.move({ from: source, to: target, promotion: piece }));
}

function tryMove(source, target) {
  if (!canInteract() || !isHumanTurn()) {
    return false;
  }

  const piece = game.get(source)?.type;
  const isPromotion =
    piece === "p" &&
    ((game.turn() === "w" && target[1] === "8") || (game.turn() === "b" && target[1] === "1"));

  if (isPromotion) {
    choosePromotion(source, target).then((move) => {
      selectedSquare = null;
      if (!move) {
        syncBoard();
        return;
      }
      afterHumanMove(move);
    });
    return true;
  }

  const move = game.move({ from: source, to: target, promotion: "q" });
  selectedSquare = null;
  if (!move) {
    syncBoard();
    return false;
  }
  afterHumanMove(move);
  return true;
}

function requestNextEngineMove() {
  if (currentMode() !== "engine") {
    return false;
  }

  if (!isLivePosition()) {
    stepHistory(1);
    return true;
  }

  if (game.game_over() || !isEngineTurn()) {
    return true;
  }

  stopAutoPlay();
  engineStepPending = true;
  if (!engineBusy) {
    void applyEngineMove();
  } else {
    const side = game.turn();
    setStatus(`Searching (${side === "w" ? "White" : "Black"})... → plays when ready`);
  }
  return true;
}

async function applyEngineMove() {
  if (!isLivePosition() || engineBusy || game.game_over() || !isEngineTurn()) {
    return;
  }

  engineBusy = true;
  document.getElementById("engine-move").disabled = true;

  try {
    const side = game.turn();
    setStatus(`Searching (${side === "w" ? "White" : "Black"})...`);
    const data = await requestEngineMove(side);

    if (data.game_over || !data.bestmove) {
      finishGame(`Game over: ${data.result || "no move"}`);
      syncBoard();
      return;
    }

    const move = game.move(data.bestmove, { sloppy: true });
    if (!move) {
      finishGame(`Engine played illegal move: ${data.bestmove}`);
      syncBoard();
      return;
    }

    appendToHistory(move, {
      score: data.score,
      nodes: data.nodes,
      version: data.version || searchConfig(side).version,
    });

    setStatus(
      `${move.san} | score ${data.score} | nodes ${data.nodes} | v${data.version || searchConfig(side).version} | ← → to review`
    );

    if (game.game_over()) {
      if (game.in_checkmate()) finishGame("Checkmate.");
      else if (game.in_stalemate()) finishGame("Stalemate.");
      else finishGame("Draw.");
      return;
    }

    if (currentMode() !== "engine" && isEngineTurn()) {
      window.setTimeout(applyEngineMove, 100);
    }
  } catch (err) {
    setStatus(`Error: ${err.message}`);
    stopAutoPlay();
  } finally {
    engineBusy = false;
    engineStepPending = false;
    document.getElementById("engine-move").disabled = !isEngineTurn() || !isLivePosition();
  }
}

function onSquareClick(square) {
  if (!canInteract() || !isHumanTurn()) {
    return;
  }

  if (selectedSquare === null) {
    const piece = game.get(square);
    if (!piece || piece.color !== game.turn()) {
      return;
    }
    selectedSquare = square;
    showLegalMoves(square);
    return;
  }

  if (square === selectedSquare) {
    selectedSquare = null;
    applyLastMoveHighlight();
    return;
  }

  const legal = game.moves({ square: selectedSquare, verbose: true }).some((m) => m.to === square);
  if (legal) {
    tryMove(selectedSquare, square);
    return;
  }

  const piece = game.get(square);
  if (piece && piece.color === game.turn()) {
    selectedSquare = square;
    showLegalMoves(square);
    return;
  }

  selectedSquare = null;
  applyLastMoveHighlight();
}

function onDragStart(source, piece) {
  if (!canInteract() || !isHumanTurn()) return false;
  if (game.turn() === "w" && piece.startsWith("b")) return false;
  if (game.turn() === "b" && piece.startsWith("w")) return false;
  selectedSquare = source;
  showLegalMoves(source);
  return true;
}

function onDrop(source, target) {
  selectedSquare = null;
  if (!target || !canInteract() || !isHumanTurn()) return "snapback";
  if (!tryMove(source, target)) return "snapback";
}

function onMouseoverSquare() {}

function onMouseoutSquare() {}

function stopAutoPlay() {
  if (autoPlayTimer !== null) {
    window.clearInterval(autoPlayTimer);
    autoPlayTimer = null;
  }
  document.getElementById("toggle-auto").textContent = "Start auto-play";
}

function startAutoPlay() {
  stopAutoPlay();
  autoPlayTimer = window.setInterval(() => {
    if (!engineBusy && isEngineTurn() && isLivePosition()) {
      applyEngineMove();
    }
  }, Number(delayEl.value));
  document.getElementById("toggle-auto").textContent = "Stop auto-play";
}

function beginGameFromCurrentPosition(statusText) {
  clearTtOnNextSearch = true;
  initHistory();
  hideGameOver();
  syncBoard();
  setStatus(statusText);
  updateControlsForMode();
  document.getElementById("engine-move").disabled = !isEngineTurn() || !isLivePosition();
  if (isEngineTurn()) {
    applyEngineMove();
  }
}

function loadPositionFromFen() {
  stopAutoPlay();
  selectedSquare = null;

  const fen = startFenEl.value.trim();
  if (!fen) {
    setStatus("Enter a FEN, or use New game for the standard start.");
    return;
  }

  const loaded = game.load(fen);
  if (!loaded) {
    setStatus("Invalid FEN — check the string and try again.");
    return;
  }

  beginGameFromCurrentPosition(`Loaded position. ${game.turn() === "w" ? "White" : "Black"} to move.`);
}

function resetGame() {
  stopAutoPlay();
  startFenEl.value = "";
  game.reset();
  beginGameFromCurrentPosition("New game. Click a piece to see legal moves.");
}

function updateControlsForMode() {
  const mode = currentMode();
  const engineMode = mode === "engine";
  whiteVersionEl.disabled = !engineMode && mode !== "human-black";
  blackVersionEl.disabled = !engineMode && mode !== "human-white";
  document.getElementById("toggle-auto").disabled = !engineMode;
  document.getElementById("engine-move").disabled = !isEngineTurn() || !isLivePosition();
}

function initBoard() {
  board = Chessboard("board", {
    draggable: true,
    position: "start",
    pieceTheme: "img/chesspieces/wikipedia/{piece}.png",
    orientation: boardOrientation,
    onDragStart,
    onDrop,
    onSquareClick,
    onMouseoverSquare,
    onMouseoutSquare,
  });
}

function onKeyDown(event) {
  if (event.target.tagName === "INPUT" || event.target.tagName === "SELECT" || event.target.tagName === "TEXTAREA") {
    return;
  }
  if (event.key === "ArrowLeft") {
    event.preventDefault();
    stepHistory(-1);
  } else if (event.key === "ArrowRight") {
    event.preventDefault();
    if (!requestNextEngineMove()) {
      stepHistory(1);
    }
  }
}

fillVersionSelect(whiteVersionEl, 21);
fillVersionSelect(blackVersionEl, 12);

whiteVersionEl.addEventListener("change", () => {
  clearTtOnNextSearch = true;
});
blackVersionEl.addEventListener("change", () => {
  clearTtOnNextSearch = true;
});

document.getElementById("new-game").addEventListener("click", resetGame);
document.getElementById("load-fen").addEventListener("click", loadPositionFromFen);
startFenEl.addEventListener("keydown", (event) => {
  if (event.key === "Enter") {
    event.preventDefault();
    loadPositionFromFen();
  }
});
document.getElementById("game-over-new").addEventListener("click", resetGame);
document.getElementById("engine-move").addEventListener("click", applyEngineMove);
document.getElementById("flip").addEventListener("click", () => {
  boardOrientation = boardOrientation === "white" ? "black" : "white";
  board.orientation(boardOrientation);
  syncBoard();
});
document.getElementById("toggle-auto").addEventListener("click", () => {
  if (autoPlayTimer === null) {
    if (currentMode() !== "engine") {
      setStatus("Auto-play is only for Engine vs Engine mode.");
      return;
    }
    startAutoPlay();
    applyEngineMove();
  } else {
    stopAutoPlay();
  }
});
modeEl.addEventListener("change", () => {
  updateControlsForMode();
  resetGame();
});
document.addEventListener("keydown", onKeyDown);

for (const button of promotionModal.querySelectorAll("[data-piece]")) {
  button.addEventListener("click", () => completePromotion(button.dataset.piece));
}

async function checkServerVersion() {
  try {
    const response = await fetch("/api/health");
    const data = await response.json();
    const maxVersion = Number(data.max_version);
    if (!Number.isFinite(maxVersion)) {
      setStatus("Server is outdated — restart with ./scripts/run_gui.sh");
      return;
    }
    const uiMax = VERSIONS[0].id;
    if (maxVersion < uiMax) {
      setStatus(`Server supports up to v${maxVersion}; restart ./scripts/run_gui.sh for v${uiMax}`);
    }
  } catch {
    setStatus("Cannot reach engine server — run ./scripts/run_gui.sh");
  }
}

initBoard();
initHistory();
initStockfishEval({ onEval: updateStockfishDisplay });
syncBoard();
updateControlsForMode();
checkServerVersion().then(() => {
  if (statusEl.textContent.startsWith("Ready")) {
    setStatus("Ready. Click a piece for legal moves. ← → to review moves.");
  }
});
