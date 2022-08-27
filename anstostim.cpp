#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>


#define PPR			10000	// PULSES PER REV
#define MAX_RPS		100		// MAX revs per sec
#define MAX_PPS     (PPR*MAX_RPS)

#define ACC 		100     // Revs Per Sec Per Sec

class Move {
public:
	static long seq_start;
	static std::vector<Move> moves;
	const bool forwards;
	const unsigned long distance;
	const long start;

	Move(bool _fwds, unsigned long _dist, unsigned long _start):
		forwards(_fwds), distance(_dist), start(_start) {}

	static bool input(std::string& line){
		char dir;
		unsigned long dist;
		const char* def = line.c_str();
		if (sscanf(def, "%c%lu", &dir, &dist) == 2){
			switch(dir){
			case '+':
				moves.emplace_back(Move(true, dist, seq_start));
				seq_start += dist;
				return true;
			case '-':
				moves.emplace_back(Move(false, dist, seq_start));
				seq_start -= dist;
				return true;
			default:
				;
			}
		}
		return false;
	}

	void print() const {
		printf("Move: %10ld: %5lu %s\n", start, distance, forwards? "Forward": "Reverse");
	}
};
std::vector<Move> Move::moves;
long Move::seq_start;

class Trajectory {

};

int main(int argc, const char* argv[]){
	printf("anstostim\n");
	std::ifstream input_file("anstostim.dat");
	if (!input_file.is_open()) {
		std::cerr << "Could not open the file - '"
			 << "anstostim.dat" << "'" << std::endl;
		return EXIT_FAILURE;
	}

	std::string line;
	while (std::getline(input_file, line)){
		Move::input(line);
	}
	input_file.close();

	for (const Move& m : Move::moves){
		m.print();
	}

}
