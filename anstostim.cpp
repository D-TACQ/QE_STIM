#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <math.h>


#define PPR			10000	// PULSES PER REV
#define MAX_RPS		100		// MAX revs per sec
#define MAX_PPS     (PPR*MAX_RPS)

#define ACC 		10     // Revs Per Sec Per Sec

#define T_RAMP      (MAX_RPS/ACC)
#define S_RAMP      ((ACC*T_RAMP*T_RAMP)/2)

#define TPS			10000000	// Ticks Per Second minimum unit of time

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

struct Trajectory {
	unsigned uu;		// start speed
	unsigned vv; 		// end speed
	unsigned tt;        // duration in ticks
	Trajectory(unsigned _uu, unsigned _vv, unsigned _tt):
		uu(_uu), vv(_vv), tt(_tt)
	{}
	void print() const {
		printf("Trajectory: uu:%3u vv:%3u tt:%3u  => ss:%u\n", uu, vv, tt, (uu+vv)*tt/2);
	}
};

class Motion {
	std::vector<Trajectory> stages;
	const Move& move;

	Motion(const Move& _move): move(_move) {
		if (move.distance > 2*S_RAMP){
			stages.emplace_back(Trajectory(0, MAX_RPS, T_RAMP));
			stages.emplace_back(Trajectory(MAX_RPS, MAX_RPS, (move.distance-2*S_RAMP)/MAX_RPS));
			stages.emplace_back(Trajectory(MAX_RPS, 0, T_RAMP));
		}else{
			unsigned s_ramp = move.distance/2;
			unsigned t_ramp = sqrt(2*s_ramp/ACC);  // s=ut +1/2 at*t
			unsigned vmax = ACC*t_ramp;
			stages.emplace_back(Trajectory(0, vmax, t_ramp));
			stages.emplace_back(Trajectory(vmax, 0, t_ramp));
		}
	}
public:
	void print() const {
		printf("Motion of Move: "); move.print();
		for (auto s: stages){
			printf("\t"); s.print();
		}
	}

	static std::vector<Motion> motions;
	static void add(const Move& move){
		motions.emplace_back(Motion(move));
	}
};

std::vector<Motion> Motion::motions;

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
		Motion::add(m);
	}
	for (const Motion& m : Motion::motions){
		m.print();
	}
}
