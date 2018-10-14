
test1:
	g++ -o test1 test1.C modparser.C --std=c++0x -g3
test2:
	g++ -o test2 test2.C modparser.C audiorenderer.C --std=c++0x -g3 -Wall
