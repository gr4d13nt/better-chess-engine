#include "engine/book.hpp"

#include "engine/movegen.hpp"
#include "engine/polyglot.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {
namespace {

struct TextBookEntry {
  std::string uci;
  int weight = 1;
};

struct PolyglotEntry {
  std::uint64_t key = 0;
  std::uint16_t raw_move = 0;
  std::uint16_t weight = 0;
};

std::unordered_map<std::uint64_t, std::vector<TextBookEntry>> g_text_book;
std::vector<PolyglotEntry> g_polyglot_book;
bool g_use_polyglot = false;
bool g_loaded = false;

std::mt19937& book_rng() {
  static std::mt19937 rng{std::random_device{}()};
  return rng;
}

Move find_move_by_uci(const Board& board, const std::string& uci) {
  MoveList moves;
  generate_legal_moves(board, moves);
  for (int i = 0; i < moves.count; ++i) {
    if (move_to_uci(moves.moves[i]) == uci) {
      return moves.moves[i];
    }
  }
  return Move{};
}

void add_text_entry(std::uint64_t key, const std::string& uci, int weight) {
  auto& entries = g_text_book[key];
  for (auto& entry : entries) {
    if (entry.uci == uci) {
      entry.weight += weight;
      return;
    }
  }
  entries.push_back({uci, weight});
}

bool load_text_line(Board& board, const std::string& line) {
  std::istringstream stream(line);
  std::string uci;
  while (stream >> uci) {
    if (uci.empty() || uci[0] == '#') {
      break;
    }
    const std::uint64_t key = board.zobrist();
    add_text_entry(key, uci, 1);

    const Move move = find_move_by_uci(board, uci);
    if (move.from == Square::None) {
      return false;
    }
    Undo undo;
    board.make_move(move, undo);
  }
  return true;
}

bool load_text_file(const char* path) {
  std::ifstream in(path);
  if (!in) {
    return false;
  }

  Board board;
  board.set_startpos();
  std::string line;
  while (std::getline(in, line)) {
    const std::size_t comment = line.find('#');
    if (comment != std::string::npos) {
      line = line.substr(0, comment);
    }
    if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
      continue;
    }
    board.set_startpos();
    load_text_line(board, line);
  }
  return !g_text_book.empty();
}

bool load_polyglot_file(const char* path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }

  in.seekg(0, std::ios::end);
  const std::streamoff size = in.tellg();
  if (size <= 0 || size % 16 != 0) {
    return false;
  }
  in.seekg(0, std::ios::beg);

  const std::size_t count = static_cast<std::size_t>(size / 16);
  g_polyglot_book.resize(count);

  for (std::size_t i = 0; i < count; ++i) {
    std::uint8_t bytes[16];
    in.read(reinterpret_cast<char*>(bytes), 16);
    if (!in) {
      g_polyglot_book.clear();
      return false;
    }

    PolyglotEntry entry;
    entry.key = (static_cast<std::uint64_t>(bytes[0]) << 56) |
                (static_cast<std::uint64_t>(bytes[1]) << 48) |
                (static_cast<std::uint64_t>(bytes[2]) << 40) |
                (static_cast<std::uint64_t>(bytes[3]) << 32) |
                (static_cast<std::uint64_t>(bytes[4]) << 24) |
                (static_cast<std::uint64_t>(bytes[5]) << 16) |
                (static_cast<std::uint64_t>(bytes[6]) << 8) |
                static_cast<std::uint64_t>(bytes[7]);
    entry.raw_move = (static_cast<std::uint16_t>(bytes[8]) << 8) | bytes[9];
    entry.weight = (static_cast<std::uint16_t>(bytes[10]) << 8) | bytes[11];
    g_polyglot_book[i] = entry;
  }

  return !g_polyglot_book.empty();
}

bool path_ends_with(const char* path, const char* suffix) {
  const std::string p(path);
  const std::string s(suffix);
  if (p.size() < s.size()) {
    return false;
  }
  return p.compare(p.size() - s.size(), s.size(), s) == 0;
}

bool load_book_file(const char* path) {
  g_text_book.clear();
  g_polyglot_book.clear();
  g_use_polyglot = false;

  if (path == nullptr || path[0] == '\0') {
    return false;
  }

  if (path_ends_with(path, ".txt")) {
    return load_text_file(path);
  }

  if (load_polyglot_file(path)) {
    g_use_polyglot = true;
    return true;
  }

  return load_text_file(path);
}

void ensure_loaded() {
  static std::once_flag once;
  std::call_once(once, []() {
    g_loaded = true;
#ifdef OPENING_BOOK_PATH
    load_book_file(OPENING_BOOK_PATH);
#endif
  });
}

std::size_t polyglot_lower_bound(std::uint64_t key) {
  std::size_t lo = 0;
  std::size_t hi = g_polyglot_book.size();
  while (lo < hi) {
    const std::size_t mid = lo + (hi - lo) / 2;
    if (g_polyglot_book[mid].key < key) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return lo;
}

std::optional<Move> pick_weighted_polyglot(const Board& board, std::uint64_t key) {
  const std::size_t start = polyglot_lower_bound(key);
  if (start >= g_polyglot_book.size() || g_polyglot_book[start].key != key) {
    return std::nullopt;
  }

  struct Candidate {
    Move move;
    int weight = 0;
  };
  std::vector<Candidate> candidates;
  candidates.reserve(8);

  for (std::size_t i = start; i < g_polyglot_book.size() && g_polyglot_book[i].key == key; ++i) {
    const auto& entry = g_polyglot_book[i];
    if (entry.weight == 0) {
      continue;
    }
    const auto move = polyglot_move_to_engine(board, entry.raw_move);
    if (!move.has_value()) {
      continue;
    }
    bool merged = false;
    for (auto& candidate : candidates) {
      if (candidate.move.from == move->from && candidate.move.to == move->to &&
          candidate.move.promotion == move->promotion && candidate.move.flag == move->flag) {
        candidate.weight += entry.weight;
        merged = true;
        break;
      }
    }
    if (!merged) {
      candidates.push_back({*move, entry.weight});
    }
  }

  if (candidates.empty()) {
    return std::nullopt;
  }

  int total = 0;
  for (const auto& candidate : candidates) {
    total += candidate.weight;
  }
  if (total <= 0) {
    return std::nullopt;
  }

  std::uniform_int_distribution<int> dist(0, total - 1);
  int pick = dist(book_rng());
  for (const auto& candidate : candidates) {
    pick -= candidate.weight;
    if (pick < 0) {
      return candidate.move;
    }
  }
  return candidates.back().move;
}

std::optional<Move> pick_weighted_text(const Board& board, std::uint64_t key) {
  const auto it = g_text_book.find(key);
  if (it == g_text_book.end() || it->second.empty()) {
    return std::nullopt;
  }

  struct Candidate {
    Move move;
    int weight = 0;
  };
  std::vector<Candidate> candidates;

  for (const auto& entry : it->second) {
    const Move move = find_move_by_uci(board, entry.uci);
    if (move.from == Square::None) {
      continue;
    }
    bool merged = false;
    for (auto& candidate : candidates) {
      if (candidate.move.from == move.from && candidate.move.to == move.to &&
          candidate.move.promotion == move.promotion && candidate.move.flag == move.flag) {
        candidate.weight += entry.weight;
        merged = true;
        break;
      }
    }
    if (!merged) {
      candidates.push_back({move, entry.weight});
    }
  }

  if (candidates.empty()) {
    return std::nullopt;
  }

  int total = 0;
  for (const auto& candidate : candidates) {
    total += candidate.weight;
  }
  if (total <= 0) {
    return std::nullopt;
  }

  std::uniform_int_distribution<int> dist(0, total - 1);
  int pick = dist(book_rng());
  for (const auto& candidate : candidates) {
    pick -= candidate.weight;
    if (pick < 0) {
      return candidate.move;
    }
  }
  return candidates.back().move;
}

}  // namespace

void init_opening_book(const char* path) {
  g_loaded = true;
  if (path != nullptr && path[0] != '\0') {
    load_book_file(path);
    return;
  }
#ifdef OPENING_BOOK_PATH
  load_book_file(OPENING_BOOK_PATH);
#endif
}

bool opening_book_loaded() {
  return g_use_polyglot ? !g_polyglot_book.empty() : !g_text_book.empty();
}

std::size_t opening_book_entry_count() {
  return g_use_polyglot ? g_polyglot_book.size() : 0;
}

std::optional<Move> probe_opening_book(const Board& board) {
  ensure_loaded();
  if (g_use_polyglot) {
    return pick_weighted_polyglot(board, polyglot_hash(board));
  }
  return pick_weighted_text(board, board.zobrist());
}

}  // namespace engine
