const VERSIONS = [
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
let pendingPromotion = null;
let selectedSquare = null;

let fenHistory = [];
let moveMeta = [];
let viewIndex = 0;

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
  sfEvalBarEl.style.width = `${result.barWidth}%`;
}

function refreshStockfishEval() {
  if (typeof requestStockfishEval !== "function") {
    return;
  }
  requestStockfishEval(fenHistory[viewIndex] || game.fen());
}

function squareClass(square) {
  return `square-${square.charAt(0)}${square.charAt(1)}`;
}

function clearHighlights() {
  $("#board [class*='square-']").removeClass(
    "highlight-selected highlight-legal highlight-capture highlight-last-from highlight-last-to"
  );
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
  return {
    fen: game.fen(),
    repetition_fens: fenHistory.slice(0, viewIndex + 1),
    depth: Number(depthEl.value),
    movetime_ms: Number(movetimeEl.value),
    version: Number(side === "w" ? whiteVersionEl.value : blackVersionEl.value),
  };
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
  clearHighlights();
  if (viewIndex === 0) {
    return;
  }
  const last = moveMeta[viewIndex - 1];
  if (!last) {
    return;
  }
  highlightSquare(last.from, "highlight-last-from");
  highlightSquare(last.to, "highlight-last-to");
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

  try {
    game.load(fen);
  } catch {
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
    stepHistory(1);
  }
}

fillVersionSelect(whiteVersionEl, 14);
fillVersionSelect(blackVersionEl, 12);

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

initBoard();
initHistory();
initStockfishEval({ onEval: updateStockfishDisplay });
syncBoard();
updateControlsForMode();
setStatus("Ready. Click a piece for legal moves. ← → to review moves.");
