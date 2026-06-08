#include "engine/attacks.hpp"
#include "engine/board.hpp"
#include "engine/move.hpp"
#include "engine/movegen.hpp"
#include "engine/repetition.hpp"
#include "engine/search.hpp"
#include "engine/zobrist.hpp"

#include <httplib.h>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace {

#ifndef CHESS_WEB_ROOT
#define CHESS_WEB_ROOT "web"
#endif

std::optional<engine::EngineVersion> parse_version(int v) {
  switch (v) {
    case 30:
      return engine::EngineVersion::V30_TimeManagement;
    case 29:
      return engine::EngineVersion::V29_LazySMP;
    case 28:
      return engine::EngineVersion::V28_FutilityLmp;
    case 27:
      return engine::EngineVersion::V27_PVS;
    case 26:
      return engine::EngineVersion::V26_LMR;
    case 25:
      return engine::EngineVersion::V25_OpeningBook;
    case 24:
      return engine::EngineVersion::V24_HangingPieces;
    case 23:
      return engine::EngineVersion::V23_Space;
    case 22:
      return engine::EngineVersion::V22_ExtendedPawnStructure;
    case 21:
      return engine::EngineVersion::V21_PassedPawns;
    case 20:
      return engine::EngineVersion::V20_PersistentTT;
    case 19:
      return engine::EngineVersion::V19_KillerHistory;
    case 18:
      return engine::EngineVersion::V18_NullMove;
    case 17:
      return engine::EngineVersion::V17_DeltaQsearch;
    case 16:
      return engine::EngineVersion::V16_SeeQsearch;
    case 15:
      return engine::EngineVersion::V15_PiecePlacement;
    case 14:
      return engine::EngineVersion::V14_RepetitionDraw;
    case 13:
      return engine::EngineVersion::V13_MopUpEval;
    case 12:
      return engine::EngineVersion::V12_CheckExtension;
    case 11:
      return engine::EngineVersion::V11_TTHashMove;
    case 10:
      return engine::EngineVersion::V10_TranspositionTable;
    case 9:
      return engine::EngineVersion::V9_RookPlacement;
    case 8:
      return engine::EngineVersion::V8_KnightOutposts;
    case 7:
      return engine::EngineVersion::V7_PhasedEval;
    case 6:
      return engine::EngineVersion::V6_PawnKingEval;
    case 5:
      return engine::EngineVersion::V5_MoveOrdering;
    case 4:
      return engine::EngineVersion::V4_Quiescence;
    case 3:
      return engine::EngineVersion::V3_ImprovedEval;
    case 2:
      return engine::EngineVersion::V2_AlphaBeta;
    case 1:
      return engine::EngineVersion::V1_NoPruning;
    default:
      return std::nullopt;
  }
}

int json_int_token(const std::string& body, const std::string& key, int fallback) {
  const std::string needle = "\"" + key + "\":";
  const std::size_t start = body.find(needle);
  if (start == std::string::npos) {
    return fallback;
  }
  std::size_t pos = start + needle.size();
  while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t')) {
    ++pos;
  }
  std::size_t end = pos;
  while (end < body.size() && body[end] >= '0' && body[end] <= '9') {
    ++end;
  }
  if (end == pos) {
    return fallback;
  }
  return std::stoi(body.substr(pos, end - pos));
}

bool json_bool_token(const std::string& body, const std::string& key, bool fallback) {
  const std::string needle = "\"" + key + "\":";
  const std::size_t start = body.find(needle);
  if (start == std::string::npos) {
    return fallback;
  }
  std::size_t pos = start + needle.size();
  while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t')) {
    ++pos;
  }
  if (body.compare(pos, 4, "true") == 0) {
    return true;
  }
  if (body.compare(pos, 5, "false") == 0) {
    return false;
  }
  return fallback;
}

std::optional<std::string> json_string(const std::string& body, const std::string& key) {
  const std::string needle = "\"" + key + "\":\"";
  const std::size_t start = body.find(needle);
  if (start == std::string::npos) {
    return std::nullopt;
  }
  const std::size_t value_start = start + needle.size();
  const std::size_t value_end = body.find('"', value_start);
  if (value_end == std::string::npos) {
    return std::nullopt;
  }
  return body.substr(value_start, value_end - value_start);
}

std::string json_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

std::vector<std::string> json_string_array(const std::string& body, const std::string& key) {
  const std::string needle = "\"" + key + "\":";
  const std::size_t start = body.find(needle);
  if (start == std::string::npos) {
    return {};
  }
  const std::size_t array_start = body.find('[', start);
  if (array_start == std::string::npos) {
    return {};
  }
  const std::size_t array_end = body.find(']', array_start);
  if (array_end == std::string::npos) {
    return {};
  }

  std::vector<std::string> out;
  std::size_t pos = array_start + 1;
  while (pos < array_end) {
    const std::size_t quote = body.find('"', pos);
    if (quote == std::string::npos || quote >= array_end) {
      break;
    }
    const std::size_t quote_end = body.find('"', quote + 1);
    if (quote_end == std::string::npos || quote_end >= array_end) {
      break;
    }
    out.push_back(body.substr(quote + 1, quote_end - quote - 1));
    pos = quote_end + 1;
  }
  return out;
}

std::string game_over_result(const engine::Board& board) {
  engine::MoveList legal;
  engine::generate_legal_moves(board, legal);
  if (legal.count > 0) {
    return "";
  }
  return board.in_check(board.side_to_move()) ? "checkmate" : "stalemate";
}

std::string handle_search(const std::string& body) {
  const auto fen_opt = json_string(body, "fen");
  if (!fen_opt.has_value() || fen_opt->empty()) {
    return R"({"ok":false,"error":"missing fen"})";
  }

  engine::Board board;
  if (!board.set_fen(*fen_opt)) {
    return R"({"ok":false,"error":"invalid fen"})";
  }

  const std::string terminal = game_over_result(board);
  if (!terminal.empty()) {
    return std::string(R"({"ok":true,"game_over":true,"result":")") + terminal +
           R"(","bestmove":null,"score":0,"nodes":0})";
  }

  const int requested_version = json_int_token(body, "version", 13);
  const auto version = parse_version(requested_version);
  if (!version.has_value()) {
    return std::string(R"({"ok":false,"error":"unknown engine version )") +
           std::to_string(requested_version) +
           "; restart ./scripts/run_gui.sh after rebuilding\"})";
  }

  engine::SearchConfig cfg{};
  cfg.depth = json_int_token(body, "depth", 6);
  cfg.movetime_ms = json_int_token(body, "movetime_ms", 0);
  cfg.version = *version;
  cfg.repetition_history = engine::repetition_hashes_from_fens(json_string_array(body, "repetition_fens"));
  cfg.clear_transposition_table = json_bool_token(body, "clear_tt", false);
  cfg.use_opening_book = json_bool_token(body, "use_book", true);

  const engine::SearchResult result = engine::search_best_move(board, cfg);
  if (!result.has_move) {
    return std::string(R"({"ok":true,"game_over":true,"result":"nomove","bestmove":null,"score":)") +
           std::to_string(result.score) + R"(,"nodes":)" + std::to_string(result.nodes) + "}";
  }

  return std::string(R"({"ok":true,"game_over":false,"bestmove":")") +
         engine::move_to_uci(result.best_move) + R"(","score":)" + std::to_string(result.score) +
         R"(,"nodes":)" + std::to_string(result.nodes) + R"(,"version":)" +
         std::to_string(static_cast<int>(cfg.version)) + "}";
}

}  // namespace

int main(int argc, char** argv) {
  engine::init_attacks();
  engine::init_zobrist();

  int port = 8080;
  if (argc > 1) {
    port = std::stoi(argv[1]);
  }

  const std::string web_root = CHESS_WEB_ROOT;

  httplib::Server server;

  server.set_mount_point("/", web_root.c_str());

  server.Post("/api/search", [](const httplib::Request& req, httplib::Response& res) {
    res.set_header("Content-Type", "application/json");
    try {
      res.set_content(handle_search(req.body), "application/json");
    } catch (const std::exception& ex) {
      res.status = 500;
      res.set_content(std::string(R"({"ok":false,"error":")") + json_escape(ex.what()) + "\"}",
                      "application/json");
    }
  });

  server.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
    res.set_content(R"({"ok":true,"max_version":28})", "application/json");
  });

  std::cout << "Chess engine web UI\n";
  std::cout << "  http://localhost:" << port << "/\n";
  std::cout << "  static files: " << web_root << '\n';

  if (!server.listen("127.0.0.1", port)) {
    std::cerr << "Failed to listen on port " << port << '\n';
    return 1;
  }
  return 0;
}
