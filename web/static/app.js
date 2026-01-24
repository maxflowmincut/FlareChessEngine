const boardEl = document.getElementById("board");
const movesEl = document.getElementById("moves");
const statusEl = document.getElementById("status");
const messageEl = document.getElementById("message");
const statusDotEl = document.getElementById("status-dot");
const lastUpdateEl = document.getElementById("last-update");
const noteEl = document.getElementById("note");
const fenEl = document.getElementById("fen");
const fenLabelEl = document.getElementById("fen-label");
const moveCountEl = document.getElementById("move-count");
const turnEl = document.getElementById("turn");
const newGameBtn = document.getElementById("new-game");

const pieceMap = {
  P: "♙",
  N: "♘",
  B: "♗",
  R: "♖",
  Q: "♕",
  K: "♔",
  p: "♟",
  n: "♞",
  b: "♝",
  r: "♜",
  q: "♛",
  k: "♚",
};

let socket = null;
let selectedSquare = null;
let playerColor = "w";
let state = {
  board: new Map(),
  turn: "w",
  moves: [],
  waiting: true,
};

const squareEls = new Map();

function fileToChar(file) {
  return String.fromCharCode(97 + file);
}

function rankOf(square) {
  return parseInt(square[1], 10);
}

function isWhitePiece(piece) {
  return piece && piece === piece.toUpperCase();
}

function isPlayerPiece(piece) {
  if (!piece) {
    return false;
  }
  return playerColor === "w" ? isWhitePiece(piece) : piece === piece.toLowerCase();
}

function isPromotionSquare(square) {
  if (playerColor === "w") {
    return rankOf(square) === 8;
  }
  return rankOf(square) === 1;
}

function isPawn(piece) {
  return piece === "P" || piece === "p";
}

function parseFen(fen) {
  const parts = fen.trim().split(/\s+/);
  const boardPart = parts[0];
  const turn = parts[1];
  const board = new Map();

  let rank = 8;
  let file = 0;
  for (const ch of boardPart) {
    if (ch === "/") {
      rank -= 1;
      file = 0;
      continue;
    }
    if (ch >= "1" && ch <= "8") {
      file += parseInt(ch, 10);
      continue;
    }
    const square = `${fileToChar(file)}${rank}`;
    board.set(square, ch);
    file += 1;
  }

  return { board, turn };
}

function buildBoard() {
  boardEl.innerHTML = "";
  squareEls.clear();

  const ranks = [];
  const files = [];
  if (playerColor === "w") {
    for (let rank = 8; rank >= 1; rank -= 1) {
      ranks.push(rank);
    }
    for (let file = 0; file < 8; file += 1) {
      files.push(file);
    }
  } else {
    for (let rank = 1; rank <= 8; rank += 1) {
      ranks.push(rank);
    }
    for (let file = 7; file >= 0; file -= 1) {
      files.push(file);
    }
  }

  let index = 0;
  for (const rank of ranks) {
    for (const file of files) {
      const square = `${fileToChar(file)}${rank}`;
      const el = document.createElement("div");
      el.className = `square ${(file + rank) % 2 === 0 ? "light" : "dark"}`;
      el.dataset.square = square;
      el.style.setProperty("--i", index.toString());
      el.addEventListener("click", () => onSquareClick(square));
      boardEl.appendChild(el);
      squareEls.set(square, el);
      index += 1;
    }
  }
}

function renderBoard() {
  for (const [square, el] of squareEls) {
    const piece = state.board.get(square);
    const display = piece ? pieceMap[piece] : "";
    el.textContent = display;
    el.classList.remove("white-piece", "black-piece", "selected");
    if (piece) {
      el.classList.add(isWhitePiece(piece) ? "white-piece" : "black-piece");
    }
  }
  if (selectedSquare && squareEls.has(selectedSquare)) {
    squareEls.get(selectedSquare).classList.add("selected");
  }
}

function renderMoves() {
  movesEl.innerHTML = "";
  if (state.moves.length === 0) {
    const empty = document.createElement("div");
    empty.className = "move-number";
    empty.textContent = "—";
    movesEl.appendChild(empty);

    const emptyWhite = document.createElement("div");
    emptyWhite.className = "move-cell";
    emptyWhite.textContent = "";
    movesEl.appendChild(emptyWhite);

    const emptyBlack = document.createElement("div");
    emptyBlack.className = "move-cell";
    emptyBlack.textContent = "";
    movesEl.appendChild(emptyBlack);
    return;
  }
  for (let i = 0; i < state.moves.length; i += 2) {
    const moveNumber = Math.floor(i / 2) + 1;
    const numberCell = document.createElement("div");
    numberCell.className = "move-number";
    numberCell.textContent = `${moveNumber}.`;
    movesEl.appendChild(numberCell);

    const whiteCell = document.createElement("div");
    whiteCell.className = "move-cell";
    whiteCell.textContent = state.moves[i] || "";
    movesEl.appendChild(whiteCell);

    const blackCell = document.createElement("div");
    blackCell.className = "move-cell";
    blackCell.textContent = state.moves[i + 1] || "";
    movesEl.appendChild(blackCell);
  }
}

function setStatus(text) {
  const value = text || "";
  statusEl.textContent = value;
  statusEl.classList.toggle("status-over", value === "Game over");
  document.title = value ? `Flare Engine - ${value}` : "Flare Engine";
}

function logMessage(text) {
  messageEl.textContent = text || "";
}

function setConnectionState(state) {
  if (document.body) {
    document.body.dataset.connection = state;
  }
  if (statusDotEl) {
    statusDotEl.dataset.state = state;
  }
}

function updateNote() {
  if (!noteEl) {
    return;
  }
  const colorText = playerColor === "w" ? "White" : "Black";
  noteEl.textContent = `You are playing ${colorText}. Promotions default to a queen.`;
}

function setPlayerColor(color) {
  playerColor = color;
  selectedSquare = null;
  state.board = new Map();
  state.moves = [];
  state.turn = "w";
  state.waiting = true;
  updateNote();
  buildBoard();
  renderBoard();
  renderMoves();
  setStatus("Starting");
  logMessage("");
  if (fenEl) {
    fenEl.textContent = "";
  }
  if (fenLabelEl) {
    fenLabelEl.textContent = "Current FEN";
  }
  if (moveCountEl) {
    moveCountEl.textContent = "0 moves";
  }
  if (turnEl) {
    turnEl.textContent = "White to move";
  }
}

function clearSelection() {
  selectedSquare = null;
  renderBoard();
}

function sendMessage(payload) {
  if (!socket || socket.readyState !== WebSocket.OPEN) {
    logMessage("Not connected to the server.");
    return;
  }
  socket.send(JSON.stringify(payload));
}

function onSquareClick(square) {
  if (state.waiting || state.turn !== playerColor) {
    return;
  }

  const piece = state.board.get(square);
  if (selectedSquare) {
    if (selectedSquare === square) {
      clearSelection();
      return;
    }
    const fromPiece = state.board.get(selectedSquare);
    if (!fromPiece || !isPlayerPiece(fromPiece)) {
      clearSelection();
      return;
    }
    let uci = `${selectedSquare}${square}`;
    if (isPawn(fromPiece) && isPromotionSquare(square)) {
      uci += "q";
    }
    state.waiting = true;
    setStatus("Sending move");
    logMessage("");
    sendMessage({ type: "move", uci });
    clearSelection();
    return;
  }

  if (piece && isPlayerPiece(piece)) {
    selectedSquare = square;
    renderBoard();
  }
}

function handleState(msg) {
  if (!msg.fen) {
    return;
  }
  const parsed = parseFen(msg.fen);
  state.board = parsed.board;
  state.turn = parsed.turn;
  state.moves = msg.moves || [];
  const status = msg.status || "";
  setStatus(status);
  state.waiting = status !== "Your move";
  renderBoard();
  renderMoves();
  logMessage(msg.message || "");
  if (lastUpdateEl) {
    lastUpdateEl.textContent = new Date().toLocaleTimeString();
  }
  if (fenEl) {
    fenEl.textContent = msg.fen;
  }
  if (fenLabelEl) {
    fenLabelEl.textContent = status === "Game over" ? "Final FEN" : "Current FEN";
  }
  if (moveCountEl) {
    const fullMoves = Math.ceil(state.moves.length / 2);
    moveCountEl.textContent = `${fullMoves} moves`;
  }
  if (turnEl) {
    if (status === "Game over") {
      turnEl.textContent = "Game over";
    } else {
      turnEl.textContent = parsed.turn === "w" ? "White to move" : "Black to move";
    }
  }
}

function handleError(msg) {
  logMessage(msg.message || "Error from server");
  setStatus("Your move");
  state.waiting = false;
}

function connect() {
  setConnectionState("connecting");
  const protocol = window.location.protocol === "https:" ? "wss" : "ws";
  const url = `${protocol}://${window.location.host}/ws`;
  socket = new WebSocket(url);

  socket.addEventListener("open", () => {
    setConnectionState("connected");
    setStatus("Connected");
    sendMessage({ type: "new", color: playerColor === "w" ? "white" : "black" });
  });

  socket.addEventListener("message", (event) => {
    let msg = null;
    try {
      msg = JSON.parse(event.data);
    } catch (err) {
      logMessage("Invalid message from server");
      return;
    }
    if (msg.type === "state") {
      handleState(msg);
    } else if (msg.type === "error") {
      handleError(msg);
    }
  });

  socket.addEventListener("close", () => {
    setConnectionState("disconnected");
    setStatus("Disconnected");
    state.waiting = true;
  });

  socket.addEventListener("error", () => {
    setConnectionState("disconnected");
    logMessage("WebSocket error");
  });
}

newGameBtn.addEventListener("click", () => {
  setPlayerColor(playerColor === "w" ? "b" : "w");
  sendMessage({ type: "new", color: playerColor === "w" ? "white" : "black" });
});

setPlayerColor("w");
connect();
