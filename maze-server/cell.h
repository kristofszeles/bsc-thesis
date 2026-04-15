#pragma once

class Cell {
private:
	int x, y;
	int type;
	bool visited;
	bool topWall, bottomWall, leftWall, rightWall;
	bool finishPath;
public:
	Cell() {
		visited = false;
		topWall = bottomWall = leftWall = rightWall = true;
		x = -1;
		y = -1;
		type = 0;
		finishPath = false;
	}
	void visit() { visited = true; }
	void unVisit() { visited = false; }
	void removeTopWall() { topWall = false; }
	void removeBottomWall() { bottomWall = false; }
	void removeLeftWall() { leftWall = false; }
	void removeRightWall() { rightWall = false; }
	void setPosititon(int x, int y) {
		this->x = x;
		this->y = y;
	}
	void setFinish() { type = 1; }
	void setStart() { type = 2; }
	void setItem() { type = 3; }
	void setNPC() { type = 4; }
	void setFinishPath() { finishPath = true; }
	void unSetFinishPath() { finishPath = false; }
	int getX() const { return x; }
	int getY() const { return y; }
	bool isVisited() const { return visited; }
	bool hasTopWall() const { return topWall; }
	bool hasBottomWall() const { return bottomWall; }
	bool hasLeftWall() const { return leftWall; }
	bool hasRightWall() const { return rightWall; }
	bool isFinish() const { return type == 1; }
	bool isStart() const { return type == 2; }
	bool isItem() const { return type == 3; }
	bool isNPC() const { return type == 4; }
	bool isFinishPath() const { return finishPath || isFinish(); }
};
