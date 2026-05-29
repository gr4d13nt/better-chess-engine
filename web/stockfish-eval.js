// Browser Stockfish eval via local stockfish/stockfish.js worker (WASM).
(function () {
  const SF_DEPTH = 14;
  const EVAL_CLAMP = 5;

  let worker = null;
  let ready = false;
  let requestId = 0;
  let currentSearchId = 0;
  let pendingFen = null;
  let onEval = null;
  let onStatus = null;
  let latestEval = null;
  let activeStm = "w";

  function sideToMove(fen) {
    return fen.split(/\s+/)[1] === "b" ? "b" : "w";
  }

  function whitePerspectiveScore(type, value, stm) {
    const sign = stm === "w" ? 1 : -1;
    if (type === "mate") {
      const mate = sign * value;
      return { type: "mate", value: mate, text: formatMate(mate) };
    }
    const cp = sign * value;
    return { type: "cp", value: cp, text: formatCp(cp) };
  }

  function formatCp(cp) {
    const pawns = cp / 100;
    return (pawns >= 0 ? "+" : "") + pawns.toFixed(2);
  }

  function formatMate(mate) {
    if (mate > 0) {
      return `M${mate}`;
    }
    return `-M${Math.abs(mate)}`;
  }

  function barWidthFromEval(evalResult) {
    let pawns = 0;
    if (evalResult.type === "mate") {
      pawns = evalResult.value > 0 ? EVAL_CLAMP : -EVAL_CLAMP;
    } else {
      pawns = Math.max(-EVAL_CLAMP, Math.min(EVAL_CLAMP, evalResult.value / 100));
    }
    return 50 + (pawns / EVAL_CLAMP) * 45;
  }

  function parseInfoLine(line, stm) {
    if (!line.startsWith("info ") || !line.includes(" score ")) {
      return null;
    }
    const depthMatch = line.match(/\bdepth (\d+)/);
    const scoreMatch = line.match(/\bscore (cp|mate) (-?\d+)/);
    if (!depthMatch || !scoreMatch) {
      return null;
    }
    const depth = Number(depthMatch[1]);
    const type = scoreMatch[1];
    const value = Number(scoreMatch[2]);
    const evalResult = whitePerspectiveScore(type, value, stm);
    return { depth, ...evalResult, barWidth: barWidthFromEval(evalResult) };
  }

  function handleLine(line) {
    if (line === "uciok") {
      worker.postMessage("isready");
      return;
    }
    if (line === "readyok") {
      ready = true;
      if (onStatus) {
        onStatus("ready");
      }
      if (pendingFen) {
        const fen = pendingFen;
        pendingFen = null;
        requestStockfishEval(fen);
      }
      return;
    }

    if (currentSearchId === 0) {
      return;
    }

    const parsed = parseInfoLine(line, activeStm);
    if (parsed) {
      latestEval = parsed;
      if (onEval) {
        onEval({ ...parsed, thinking: true });
      }
    }

    if (line.startsWith("bestmove")) {
      if (latestEval && onEval) {
        onEval({ ...latestEval, thinking: false });
      }
      currentSearchId = 0;
    }
  }

  function requestStockfishEval(fen) {
    if (!fen) {
      return;
    }
    if (!worker) {
      pendingFen = fen;
      return;
    }
    if (!ready) {
      pendingFen = fen;
      return;
    }

    currentSearchId = ++requestId;
    activeStm = sideToMove(fen);
    latestEval = null;

    worker.postMessage("stop");
    worker.postMessage(`position fen ${fen}`);
    worker.postMessage(`go depth ${SF_DEPTH}`);

    if (onEval) {
      onEval({ text: "…", barWidth: 50, thinking: true });
    }
  }

  function initStockfishEval(callbacks) {
    onEval = callbacks.onEval || null;
    onStatus = callbacks.onStatus || null;

    try {
      worker = new Worker("stockfish/stockfish.js");
    } catch (err) {
      if (onStatus) {
        onStatus("failed");
      }
      if (onEval) {
        onEval({ text: "SF unavailable", barWidth: 50, thinking: false, error: true });
      }
      return;
    }

    worker.onmessage = (event) => {
      if (typeof event.data !== "string") {
        return;
      }
      handleLine(event.data);
    };

    worker.onerror = () => {
      ready = false;
      if (onStatus) {
        onStatus("failed");
      }
      if (onEval) {
        onEval({ text: "SF error", barWidth: 50, thinking: false, error: true });
      }
    };

    worker.postMessage("uci");
    if (onStatus) {
      onStatus("loading");
    }
  }

  window.initStockfishEval = initStockfishEval;
  window.requestStockfishEval = requestStockfishEval;
})();
