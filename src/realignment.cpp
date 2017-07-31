/*
Copyright (C) 2017 Melissa Gymrek <mgymrek@ucsd.edu>
and Nima Mousavi (mousavi@ucsd.edu)

This file is part of GangSTR.

GangSTR is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GangSTR is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GangSTR.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "src/realignment.h"

#include <sstream>
#include <iostream>

bool expansion_aware_realign(const std::string& seq,
			     const std::string& pre_flank,
			     const std::string& post_flank,
			     const std::string& motif,
			     int32_t* nCopy, int32_t* pos, int32_t* score) {
  int32_t read_len = (int32_t)seq.size();
  int32_t period = (int32_t)motif.size();
  int32_t max_score = 0;
  int32_t max_nCopy = 0;
  int32_t max_pos = 0;
  int32_t current_score = 0;
  int32_t current_pos = 0;
  int32_t current_nCopy;
  MARGIN = 4 * period - 1;
  for (current_nCopy=0; current_nCopy<(int32_t)(read_len/period)+2; current_nCopy++) {
    std::stringstream var_realign_ss;
    var_realign_ss << pre_flank;
    for (int i=0; i<current_nCopy; i++) {
      var_realign_ss << motif;
    }
    var_realign_ss << post_flank;
    std::string var_realign_string = var_realign_ss.str();
    if (!smith_waterman(var_realign_string, seq, &current_pos, &current_score)) {
      return false;
    }
    if (current_score > max_score) {
      max_score = current_score;
      max_nCopy = current_nCopy;
      max_pos = current_pos;
    }
    if (current_score == read_len*MATCH_SCORE) {
      break;
    }
  }
  *nCopy = max_nCopy;
  *score = max_score;
  *pos = max_pos;
  return true;
}

bool smith_waterman(const std::string& seq1,
		    const std::string& seq2,
		    int32_t* pos, int32_t* score) {
  // The scoring matrix contains an extra row and column for the gap (-), hence
  // the +1 here
  int32_t rows = (int32_t)seq1.size() + 1;
  int32_t cols = (int32_t)seq2.size() + 1;

  // Initialize the scoring matrix
  int32_t current_score;
  int32_t start_pos;
  std::vector<std::vector<int32_t> > score_matrix;
  score_matrix.resize(rows, std::vector<int32_t>(cols, 0));
  if (!create_score_matrix(rows, cols, seq1, seq2,
			   &score_matrix, &start_pos, &current_score)) {
    return false;
  }
  *pos = start_pos-(int32_t)seq2.size();
  *score = current_score;
  return true;
}

/*
  Create a matrix of scores representing trial alignments of the two sequences.
  Sequence alignment can be treated as a graph search problem. This function
  creates a graph (2D matrix) of scores, which are based on trial alignments
  of different base pairs. The path with the highest cummulative score is the
  best alignment.
 */
bool create_score_matrix(const int32_t& rows, const int32_t& cols,
			 const std::string& seq1,
			 const std::string& seq2,
			 std::vector<std::vector<int32_t> >* score_matrix,
			 int32_t* start_pos, int32_t* current_score) {
  int32_t max_score = 0;
  int32_t max_pos_row = -1; // The row and column of highest score in the matrix
  int32_t max_pos_col = -1;
  for (int32_t i=1; i<rows; i++) {
    for (int32_t j=1; j<cols; j++) {
      if (!calc_score(i, j, seq1, seq2, score_matrix)) {
	return false;
      }
      if (score_matrix->at(i).at(j) > max_score) {
	max_score = score_matrix->at(i).at(j);
	max_pos_row = i;
	max_pos_col = j;
      }
    }
  }
  if (max_pos_row == -1 || max_pos_col == -1) {
    *current_score = 0;
    *start_pos = -1;
    return true;
  }
  *current_score = max_score;
  *start_pos = max_pos_row;
  return true;
}

/*
  Calculate score for a given x, y position in the scoring matrix.
  The score is based on the up, left, and upper-left neighbors.
 */
bool calc_score(const int32_t& i, const int32_t& j,
		const std::string& seq1, const std::string& seq2,
		std::vector<std::vector<int32_t> >* score_matrix) {
  int32_t max_score = 0;
  int32_t similarity = (seq1.at(i-1)==seq2.at(j-1)) ? 
    MATCH_SCORE : MISMATCH_SCORE;
  int32_t diag_score = score_matrix->at(i-1).at(j-1) + similarity;
  if (diag_score > max_score) {
    max_score = diag_score;
  }
  int32_t up_score = score_matrix->at(i-1).at(j) + GAP_SCORE;
  if (up_score > max_score) {
    max_score = up_score;
  }
  int32_t left_score = score_matrix->at(i).at(j-1) + GAP_SCORE;
  if (left_score > max_score) {
    max_score = left_score;
  }
  score_matrix->at(i).at(j) = max_score;
  return true;
}

bool classify_realigned_read(const std::string& seq,
			     const std::string& motif,
			     const int32_t& start_pos,
			     const int32_t& nCopy,
			     const int32_t& score,
			     const int32_t& prefix_length,
			     SingleReadType* single_read_class) {
  int32_t end_pos = start_pos + (int32_t)seq.size() - 1;

  // Get coords of the STR
  int32_t start_str = prefix_length;
  int32_t end_str = prefix_length + nCopy*(int32_t)motif.size();

  // Check if read starts in the STR
  bool start_in_str = false;
  bool end_in_str = false;
  if ((start_pos >= start_str-MARGIN) && (start_pos <= end_str+MARGIN)) {
    start_in_str = true;
  }
  if ((end_pos >= start_str-MARGIN) && (end_pos <= end_str+MARGIN)) {
    end_in_str = true;
  }

  // Set threshold for match
  int32_t score_threshold = (int32_t)(MATCH_PERC_THRESHOLD*seq.size()*MATCH_SCORE);

  if (score < score_threshold || nCopy == 0) {
    *single_read_class = SR_UNKNOWN;
    return true;
  } else if (start_in_str && end_in_str) {
    *single_read_class = SR_IRR;
    return true;
  } else if (start_in_str && !end_in_str) {
    *single_read_class = SR_POSTFLANK;
    return true;
  } else if (!start_in_str && end_in_str) {
    *single_read_class = SR_PREFLANK;
    return true;
  } else if (start_pos < start_str && end_pos > end_str) {
    *single_read_class = SR_ENCLOSING;
    return true;
  } else {
    return false;
  }
}
