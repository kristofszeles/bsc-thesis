#include <fstream>
#include <sstream>
#include <stack>
#include <list>

#include "base64.h"
#include "maze.h"

Maze::Maze(int width, int height) : width(width), height(height) {
	if (width > MAX_MAZE_WIDTH) this->width = MAX_MAZE_WIDTH;
	if (height > MAX_MAZE_HEIGHT) this->height = MAX_MAZE_HEIGHT;
	if (width < 2) this->width = 2;
	if (height < 2) this->height = 2;
	totalCells = width * height;
	mazeCells = new Cell** [height];
	for (int i = 0; i < height; ++i) {
		mazeCells[i] = new Cell * [width];
		for (int j = 0; j < width; ++j) {
			mazeCells[i][j] = new Cell();
			mazeCells[i][j]->setPosititon(j, i);
		}
	}
	startX = 0;
	startY = 0;
	endX = width - 1;
	endY = height - 1;
	totalNpcs = 0;
	randomizeDfs(startX, startY, endX, endY);
	backtrackIter(startX, startY, endX, endY);
	generateSolutionPath();
	generateEntities();
}

Maze::~Maze() {
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			delete mazeCells[i][j];
		}
		delete[] mazeCells[i];
	}
	delete[] mazeCells;
}

std::list<std::pair<int, int>> Maze::getSolutionPath() const {
	std::list<std::pair<int, int>> result;
	int axis = 0; // 1: x-axis, 2: y-axis
	for (unsigned int i = 0; i < solutionPath.size() - 1; ++i) {
		if (axis == 1) {
			if (solutionPath[i].second != solutionPath[i + 1].second) {
				result.push_back(std::pair<int, int>(solutionPath[i].first, solutionPath[i].second));
				axis = 0;
			}
		}
		if (axis == 2) {
			if (solutionPath[i].first != solutionPath[i + 1].first) {
				result.push_back(std::pair<int, int>(solutionPath[i].first, solutionPath[i].second));
				axis = 0;
			}
		}
		if (solutionPath[i].first != solutionPath[i + 1].first) {
			axis = 1;
		}
		if (solutionPath[i].second != solutionPath[i + 1].second) {
			axis = 2;
		}
	}
	result.push_back(std::pair<int, int>(solutionPath.back().first, solutionPath.back().second));
	return result;
}

void Maze::generateEntities() {
	std::stringstream output;
	float playerAngle = 0;
	output << 1;
	for (int j = 0; j < width; ++j) {
		if (mazeCells[0][j]->hasTopWall()) output << 1;
		else output << 0;
		output << 1;
	}
	output << std::endl;
	for (int i = 0; i < height; ++i) {
		if (mazeCells[i][0]->hasLeftWall()) output << 1;
		else output << 0;
		for (int j = 0; j < width; ++j) {
			if (mazeCells[i][j]->isFinish()) output << 'F';
			else if (mazeCells[i][j]->isStart()) {
				output << 'S';
				if (!mazeCells[i][j]->hasTopWall()) playerAngle = 0;
				else if (!mazeCells[i][j]->hasBottomWall()) playerAngle = 180;
				else if (!mazeCells[i][j]->hasLeftWall()) playerAngle = 90;
				else if (!mazeCells[i][j]->hasRightWall()) playerAngle = 270;
			} else if (mazeCells[i][j]->isItem()) {
				output << 'I';
			} else if (mazeCells[i][j]->isNPC()) {
				output << 'N';
			}
			else output << 0;
			if (mazeCells[i][j]->hasRightWall()) output << 1;
			else output << 0;
		}
		output << std::endl;
		output << 1;
		for (int j = 0; j < width; ++j) {
			if (mazeCells[i][j]->hasBottomWall()) output << 1;
			else output << 0;
			output << 1;
		}
		output << std::endl;
	}
	output.seekg(output.beg);
	output >> std::noskipws;
	char c;
	float x = 0, y = 0;
	while (output >> c) {
		std::string type;
		if (c == '1') type = "Tile";
		else if (c == 'S') type = "Start";
		else if (c == 'F') type = "Finish";
		else if (c == 'N') type = "NPC";
		else if (c == 'I') type = "Item" + std::to_string(rand() % 6 + 1);
		if (!type.empty()) data << type << " " << x * 3 << " " << 0 << " " << y * 3 << " " << (type != "Start" ? 0 : playerAngle) << std::endl;
		if (c == '\n') {
			x = 0;
			++y;
		} else {
			++x;
		}
	}
}

void Maze::save(const std::string& fileName) {
	std::ofstream file;
	file.open(fileName);
	file << data.rdbuf();
	file.close();
}

void Maze::randomizeDfs(int startX, int startY, int endX, int endY) {
	Cell* initialCell = mazeCells[startY][startX];
	initialCell->visit();
	initialCell->setStart();
	std::stack<Cell*> stack;
	stack.push(initialCell);
	while (!stack.empty()) {
		Cell* currentCell = stack.top();
		stack.pop();
		if (!currentCell->isStart()) {
			switch (rand() % 10) {
			case 0:
				putItem(currentCell);
				break;
			case 1:
				putNPC(currentCell);
				break;
			}
		}
		std::vector<Cell*> neighbors = getUnvisitedNeighbors(currentCell);
		if (!neighbors.empty()) {
			Cell* cell = neighbors[rand() % neighbors.size()];
			cell->visit();
			removeWallBetween(currentCell, cell);
			stack.push(currentCell);
			stack.push(cell);
		}
	}
}

void Maze::backtrackIter(int startX, int startY, int endX, int endY) {
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			mazeCells[i][j]->unVisit();
		}
	}
	Cell* currentCell = mazeCells[startY][startX];
	Cell* finishCell = mazeCells[endY][endX];
	currentCell->visit();
	finishCell->setFinish();
	std::stack<Cell*> stack;
	while (currentCell != finishCell) {
		std::vector<Cell*> neighbors = getUnvisitedAccessibleNeighbors(currentCell);
		if (!neighbors.empty()) {
			Cell* cell = neighbors[rand() % neighbors.size()];
			cell->visit();
			currentCell->setFinishPath();
			stack.push(currentCell);
			currentCell = cell;
			continue;
		}
		currentCell->unSetFinishPath();
		currentCell = stack.top();
		stack.pop();
	}
}

void Maze::generateSolutionPath() {
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			mazeCells[i][j]->unVisit();
		}
	}
	Cell* currentCell = mazeCells[startY][startX];
	while (currentCell != nullptr) {
		std::pair<int, int> position(currentCell->getX(), currentCell->getY());
		currentCell->visit();
		solutionPath.push_back(position);
		currentCell = getSolutionNextStep(currentCell);
	}
}

void Maze::removeWallBetween(Cell* cell1, Cell* cell2) {
	if (cell1->getX() == cell2->getX() && cell1->getY() == cell2->getY() - 1) {
		cell1->removeBottomWall();
		cell2->removeTopWall();
	} else if (cell1->getX() == cell2->getX() && cell1->getY() == cell2->getY() + 1) {
		cell1->removeTopWall();
		cell2->removeBottomWall();
	} else if (cell1->getX() == cell2->getX() - 1 && cell1->getY() == cell2->getY()) {
		cell1->removeRightWall();
		cell2->removeLeftWall();
	} else if (cell1->getX() == cell2->getX() + 1 && cell1->getY() == cell2->getY()) {
		cell1->removeLeftWall();
		cell2->removeRightWall();
	}
}

bool Maze::isWallBetween(Cell* cell1, Cell* cell2) const {
	if (cell1->getX() == cell2->getX() && cell1->getY() == cell2->getY() - 1) {
		if (cell1->hasBottomWall() && cell2->hasTopWall()) return true;
	} else if (cell1->getX() == cell2->getX() && cell1->getY() == cell2->getY() + 1) {
		if (cell1->hasTopWall() && cell2->hasBottomWall()) return true;
	} else if (cell1->getX() == cell2->getX() - 1 && cell1->getY() == cell2->getY()) {
		if (cell1->hasRightWall() && cell2->hasLeftWall()) return true;
	} else if (cell1->getX() == cell2->getX() + 1 && cell1->getY() == cell2->getY()) {
		if (cell1->hasLeftWall() && cell2->hasRightWall()) return true;
	}
	return false;
}

Cell* Maze::getSolutionNextStep(Cell* cell) const {
	Cell* result = nullptr;
	int i = cell->getY();
	int j = cell->getX();
	if (i - 1 >= 0 && !mazeCells[i - 1][j]->isVisited() && mazeCells[i - 1][j]->isFinishPath() && !isWallBetween(cell, mazeCells[i - 1][j])) result = mazeCells[i - 1][j]; // top
	else if (i + 1 < height && !mazeCells[i + 1][j]->isVisited() && mazeCells[i + 1][j]->isFinishPath() && !isWallBetween(cell, mazeCells[i + 1][j])) result = mazeCells[i + 1][j]; // bottom
	else if (j - 1 >= 0 && !mazeCells[i][j - 1]->isVisited() && mazeCells[i][j - 1]->isFinishPath() && !isWallBetween(cell, mazeCells[i][j - 1])) result = mazeCells[i][j - 1]; // left
	else if (j + 1 < width && !mazeCells[i][j + 1]->isVisited() && mazeCells[i][j + 1]->isFinishPath() && !isWallBetween(cell, mazeCells[i][j + 1])) result = mazeCells[i][j + 1]; // right
	return result;
}

std::vector<Cell*> Maze::getUnvisitedNeighbors(Cell* cell) const {
	std::vector<Cell*> result;
	int i = cell->getY();
	int j = cell->getX();
	if (i - 1 >= 0 && !mazeCells[i - 1][j]->isVisited()) result.push_back(mazeCells[i - 1][j]); // top
	if (i + 1 < height && !mazeCells[i + 1][j]->isVisited()) result.push_back(mazeCells[i + 1][j]); // bottom
	if (j - 1 >= 0 && !mazeCells[i][j - 1]->isVisited()) result.push_back(mazeCells[i][j - 1]); // left
	if (j + 1 < width && !mazeCells[i][j + 1]->isVisited()) result.push_back(mazeCells[i][j + 1]); // right
	return result;
}

std::vector<Cell*> Maze::getUnvisitedAccessibleNeighbors(Cell* cell) const {
	std::vector<Cell*> result;
	int i = cell->getY();
	int j = cell->getX();
	if (i - 1 >= 0 && !mazeCells[i - 1][j]->isVisited() && !isWallBetween(cell, mazeCells[i - 1][j])) result.push_back(mazeCells[i - 1][j]); // top
	if (i + 1 < height && !mazeCells[i + 1][j]->isVisited() && !isWallBetween(cell, mazeCells[i + 1][j])) result.push_back(mazeCells[i + 1][j]); // bottom
	if (j - 1 >= 0 && !mazeCells[i][j - 1]->isVisited() && !isWallBetween(cell, mazeCells[i][j - 1])) result.push_back(mazeCells[i][j - 1]); // left
	if (j + 1 < width && !mazeCells[i][j + 1]->isVisited() && !isWallBetween(cell, mazeCells[i][j + 1])) result.push_back(mazeCells[i][j + 1]); // right
	return result;
}
