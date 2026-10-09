#pragma once

// Word-boundary text fitting shared by the Solum and Quartum Home themes.
//
// Behaviour is ported from the minuta reference fork
// (MIT, Copyright (c) 2025 Dave Allie) — https://github.com/hiitsalice/minuta,
// src/components/themes/minuta/solum.cpp and quartum.cpp.
//
// These are free functions in a theme-local header rather than a new lib/ helper:
// only these two themes need word-boundary fitting, and dragging GfxRenderer into
// a shared library would widen the dependency for no gain.

#include <GfxRenderer.h>

#include <string>
#include <vector>

namespace MinutaTextLayout {

constexpr const char* kEllipsis = "...";

// Returns `text` unchanged when it fits within maxWidth. Otherwise drops trailing
// words until `shortened + "..."` fits, and falls back to a glyph-level truncation
// when even the first word is wider than the available space.
inline std::string truncateAtWord(const GfxRenderer& renderer, const int fontId, const std::string& text,
                                  const int maxWidth, const EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  if (maxWidth <= 0 || text.empty()) {
    return std::string();
  }
  if (renderer.getTextWidth(fontId, text.c_str(), style) <= maxWidth) {
    return text;
  }

  std::string shortened = text;
  while (true) {
    const size_t space = shortened.find_last_of(' ');
    if (space == std::string::npos) {
      return renderer.truncatedText(fontId, text.c_str(), maxWidth, style);
    }
    shortened.erase(space);
    if (renderer.getTextWidth(fontId, (shortened + kEllipsis).c_str(), style) <= maxWidth) {
      return shortened + kEllipsis;
    }
  }
}

// Wraps `text` into at most two lines at word boundaries: the first line is filled
// greedily, then the remainder is either placed whole on the second line or filled
// with a trailing ellipsis while words remain.
inline std::vector<std::string> wrapTwoLines(const GfxRenderer& renderer, const int fontId, const std::string& text,
                                             const int maxWidth,
                                             const EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  std::vector<std::string> lines;
  if (maxWidth <= 0 || text.empty()) {
    return lines;
  }

  // Split on runs of spaces; empty words are dropped so the loop always advances.
  std::vector<std::string> words;
  size_t pos = 0;
  while (pos < text.size()) {
    while (pos < text.size() && text[pos] == ' ') {
      ++pos;
    }
    if (pos >= text.size()) {
      break;
    }
    const size_t end = text.find(' ', pos);
    words.push_back(text.substr(pos, end == std::string::npos ? std::string::npos : end - pos));
    if (end == std::string::npos) {
      break;
    }
    pos = end + 1;
  }
  if (words.empty()) {
    return lines;
  }

  std::string first;
  size_t wordIndex = 0;
  while (wordIndex < words.size()) {
    const std::string candidate = first.empty() ? words[wordIndex] : first + " " + words[wordIndex];
    if (renderer.getTextWidth(fontId, candidate.c_str(), style) > maxWidth) {
      break;
    }
    first = candidate;
    ++wordIndex;
  }
  if (first.empty()) {
    // A single word wider than the slot: truncate it and stop.
    first = renderer.truncatedText(fontId, words[0].c_str(), maxWidth, style);
    wordIndex = 1;
  }
  lines.push_back(first);

  if (wordIndex >= words.size()) {
    return lines;
  }

  std::string remaining;
  for (size_t i = wordIndex; i < words.size(); ++i) {
    if (!remaining.empty()) {
      remaining += " ";
    }
    remaining += words[i];
  }
  if (renderer.getTextWidth(fontId, remaining.c_str(), style) <= maxWidth) {
    lines.push_back(remaining);
    return lines;
  }

  std::string second;
  while (wordIndex < words.size()) {
    const std::string candidate = second.empty() ? words[wordIndex] : second + " " + words[wordIndex];
    const bool moreWordsRemain = wordIndex + 1 < words.size();
    const std::string measured = candidate + (moreWordsRemain ? kEllipsis : "");
    if (renderer.getTextWidth(fontId, measured.c_str(), style) > maxWidth) {
      break;
    }
    second = candidate;
    ++wordIndex;
  }
  if (second.empty()) {
    lines.push_back(renderer.truncatedText(fontId, remaining.c_str(), maxWidth, style));
  } else {
    lines.push_back(second + kEllipsis);
  }
  return lines;
}

}  // namespace MinutaTextLayout
