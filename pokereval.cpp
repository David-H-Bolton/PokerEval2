#include "hr_time.h"
#include <array>
#include <fstream>
#include <future>
#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <vector>
//#include <Synchapi.h>

enum class Suit { Diamonds, Hearts, Clubs, Spades };
enum class Rank { Two=2, Three, Four, Five, Six, Seven, Eight, Nine, Ten, Jack, Queen, King,Ace };

enum class Value { HighCard=100, Pair=200, TwoPair=300, Three=400, Straight=500, Flush=600, FullHouse=700, Four=800, StraightFlush=900};
static std::string ValueStr[] = { "HighCard", "Pair", "Two Pair", "Three", "Straight", "Flush", "Full House", "Four", "Straight Flush"};
static std::string SuitStr[]={"D","H","C","S"};
static std::string RankStr[]={ "2", "3", "4", "5","6","7", "8", "9", "T", "J", "Q", "K", "A"};
const std::string PokerSuitStrings = "DHCS";
const std::string PokerSuitRanks = "23456789TJQKA";
const int MaxThreads = 12;

struct SuitCounter {
	Suit suit;
	INT8 count;
};

struct RankCounter {
	Rank rank;
	INT8 count;
};

class PokerCard {

private:	
	// convert char eg S to spades enum
	static Suit getSuitFromChar(char c) {
		std::size_t found = PokerSuitStrings.find(c); // DHCS
		return static_cast<Suit>(found);
	}

	static Rank getRankFromChar(char c) {
		std::size_t found = PokerSuitRanks.find(c); // 23456789TJQKA
		return static_cast<Rank>(found+2);
	}

public:
	Suit suit;
	Rank rank;

	PokerCard(std::string cardtext) { // RankSuit e.g. 6C = Six Clubs
		rank = getRankFromChar(cardtext[0]);
		suit = getSuitFromChar(cardtext[1]);
	}

	PokerCard() {

	}

	std::string ToString() { return RankStr[(int)rank-2] + SuitStr[(int)suit];}; // output 7D for Seven Diamonds
};

class PokerHand {
	std::array<PokerCard, 5> hand; // needs PokerCard default Constructor
	void SortByRank();
	static bool SortByRankComparison(const PokerCard &lhs, const PokerCard &rhs) { return lhs.rank < rhs.rank; }
	std::vector<RankCounter> ranklist;
	static int handVal;
public:
	PokerHand() {};
	PokerHand(std::string handtext);
	std::string GetResult(Value & handvalue);
	void WriteResult(std::ofstream& stream, Value handvalue);
	Value EvaluateHand();
	std::string ToString();

};

int PokerHand::handVal = 0;

PokerHand::PokerHand(std::string handtext) {
	auto offset = 0;
	auto handoffset = 0;
	for (auto cardIndex = 0; cardIndex < 5; cardIndex++) {
		std::string cardStr(handtext, offset, 2);
		offset += 2;
		PokerCard card(cardStr);
		hand[handoffset++] = card;
	}
	SortByRank();
}


void PokerHand::SortByRank() {
	sort(hand.begin(), hand.end(), SortByRankComparison);
}

// output hand as sorted text
std::string PokerHand::ToString() {
	std::string result = "";
	for (auto card : hand) {
		result += card.ToString(); 
	}
	return result;
}

// Friend function
Value PokerHand::EvaluateHand() {
	auto incced = false;

	auto firstsuit = hand[0].suit;
	auto isFlush = true;	
	auto isStraight = true;

	auto strDiff = 1;
	auto strRank = -1;
	auto index = 0;
	for (auto& card : hand) {
		if (isFlush && card.suit != firstsuit) {
			isFlush = false;
		}			
		// for straights- cards must differ by 1
		if (isStraight) {
			if (strRank == -1) {
				strRank = (int)card.rank; // first card
			}
			else {
				strDiff = (int)card.rank - strRank;
				// special case for A2345 where A is last card
				if (strDiff != 1 && card.rank != Rank::Ace && index != 4) {
					isStraight = false;
				}
				else {
					strRank = (int)card.rank;
				}
			}
		}
		index++;
		incced = false;
		for (auto i = 0; i < (int)ranklist.size(); i++) {

			if (card.rank == ranklist[i].rank) {
				ranklist[i].count++;
				incced = true;
				break;
			}
		}
		if (!incced) {
			RankCounter rc;
			rc.rank = card.rank;
			rc.count = 1;
			ranklist.emplace_back(rc); // faster way to add an item to a list
		}
	}

	// check straight, Flush and straight flush. If none of these then its a high card

	if (ranklist.size() == 5) {
		auto diff = (int)ranklist[4].rank - (int)ranklist[0].rank; // could be 23456 so diff of 6 or 2345A (diff of 12)
		if (isStraight) {
			isStraight = (diff == 4 || diff == 12);
		}

		if (isStraight && isFlush) {
			handVal = (int)Value::StraightFlush + (int)hand[4].rank;
			return Value::StraightFlush;		
		}
		if (isStraight) {
			handVal = (int)(Value::Straight) + (diff == 4 ? (int)ranklist[4].rank : (int)Rank::Five); // last card of 23456
			return Value::Straight;
		}
		if (isFlush) {
			handVal = handVal = (int)Value::Flush + (int)hand[4].rank;
			return Value::Flush;
		}
		// if here then its a high card
		// as most hands are high cards- this is an optimization exiting early
		handVal = (int)Value::HighCard + (int)ranklist[4].rank;
		return Value::HighCard;
	}

	if (ranklist.size() == 4) { // Pair
		for (auto i = 0; i < 4;i++) {
			if (ranklist[i].count == 2) {
				handVal = (int)Value::Pair + (int)ranklist[i].rank;
				return Value::Pair;
			}
		}
	}

	if (ranklist.size() == 3) { // two pair or trips
		auto firstPair = true;
		auto highpair = 0;
		for (auto i = 0; i < 3; i++) { // trips
			if (ranklist[i].count == 3) {
				handVal = (int)Value::Three + (int)ranklist[i].rank;
				return Value::Three;
			}
			if (ranklist[i].count == 2) {
				if ((int)ranklist[i].rank > (int)highpair) {
					highpair = (int)ranklist[i].rank;
					if (firstPair) { // first pair
						firstPair = false;
					}
					else { // two pairs
						handVal = (int)Value::TwoPair + (int)highpair;
						return Value::TwoPair;
					}
				}
			}

		}
	}

	if (ranklist.size() == 2) { // quads or Full House
		for (auto i = 0; i < 2; i++) { // trips
			if (ranklist[i].count <= 2) continue; // single card, next is quads or a pair and next is trips
			if (ranklist[i].count == 4) {
				handVal = (int)Value::Four + (int)ranklist[i].rank;
				return Value::Four;
			}
			if (ranklist[i].count == 3) { // Fullhouse
				handVal = (int)Value::FullHouse + (int)ranklist[i].rank;
				return Value::FullHouse;
			}
		}
	}

	// should never get here! This is just to keep the compiler happy!
	handVal = (int)Value::HighCard + (int)ranklist[4].rank;
	return Value::HighCard;
}

void PokerHand::WriteResult(std::ofstream& stream,Value handvalue) {
	stream << GetResult( handvalue ) << std::endl;
}

std::string PokerHand::GetResult( Value & handvalue) {
	return "("+ std::to_string(handVal)+") "+ToString() + " " + ValueStr[((int)handvalue/100)-1];
}


int main()
{
	CStopWatch sw;
	sw.startTimer();
	std::ofstream fileout("results.txt");
	std::ifstream filein("hands.txt");
	std::string str;
	auto rowCount = 0;
	std::vector<std::string> allcards;

	while (std::getline(filein, str))
	{
		allcards.push_back(str);
	}	
	//std::cout << allcards.size() << std::endl;
	filein.close();
	for (auto st : allcards){
		PokerHand pokerhand(st);
		auto result = pokerhand.EvaluateHand();
		//std::cout << pokerhand.GetResult(result ) << std::endl;
		pokerhand.WriteResult(fileout, result);
		rowCount++;
	}

	fileout.close();

	sw.stopTimer();
	std::cout << "Time to evaluate " << rowCount << " poker hands: " << sw.getElapsedTime() << std::endl;
	return 0;
}
