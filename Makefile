# mincdp -- build the demos and run the regression suite. The browser layers
# need a headless Chrome/Chromium on PATH; without one they SKIP (exit 77).
.PHONY: c java demo demo-c demo-java ut shot clean

c:
	cc -std=c99 -Wall -Wextra -O2 -o c/demo c/demo.c

java:
	javac -d java/out java/Cdp.java java/Demo.java

demo-c: c
	sh demo.sh c/demo

demo-java: java
	sh demo.sh java -cp java/out Demo

demo: demo-c demo-java

# ut -- the whole regression suite: codeut (browser-free units) + uiut (the
# client driving real Chrome: interaction "hands" + a screenshot "eyes").
ut:
	sh tests/run.sh

# shot -- just the eyes: screenshot page.html so you can SEE it.
shot:
	sh tests/shot.sh

clean:
	rm -f c/demo tests/codeut
	rm -rf java/out
