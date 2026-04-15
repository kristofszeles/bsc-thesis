#pragma once

#include <string>
#include <vector>

#include "cell.h"

class Maze {
private:
	const int MAX_MAZE_WIDTH = 100;
	const int MAX_MAZE_HEIGHT = 100;
	const int MAX_NPC_COUNT = 10;
	int width, height;
	int startX, startY, endX, endY;
	int totalCells;
	int totalNpcs;
	Cell*** mazeCells;
	std::stringstream data;
	std::vector<std::pair<int, int>> solutionPath;
	void backtrackIter(int startX, int startY, int endX, int endY);
	void generateEntities();
	void randomizeDfs(int startX, int startY, int endX, int endY);
	void save(const std::string& fileName);
	void generateSolutionPath();
	void removeWallBetween(Cell* cell1, Cell* cell2);
	void putItem(Cell* cell) { cell->setItem(); }
	void putNPC(Cell* cell) {
		if (totalNpcs < MAX_NPC_COUNT) {
			cell->setNPC();
			++totalNpcs;
		}
	}
	bool isWallBetween(Cell* cell1, Cell* cell2) const;
	int getWidth() const { return width; }
	int getHeight() const { return height; }
	int getStartX() const { return startX; }
	int getStartY() const { return startY; }
	int getEndX() const { return endX; }
	int getEndY() const { return endY; }
	Cell* getSolutionNextStep(Cell* cell) const;
	std::vector<Cell*> getUnvisitedNeighbors(Cell* cell) const;
	std::vector<Cell*> getUnvisitedAccessibleNeighbors(Cell* cell) const;
public:
	Maze(int width, int height);
	~Maze();
	std::stringstream& getData() { return data; }
	std::list<std::pair<int, int>> getSolutionPath() const;
};
