# mincdp -- build and run the C and Java demos. Each demo needs a headless
# Chrome/Chromium on PATH; without one, demo.sh SKIPs (exit 77).
.PHONY: c java demo demo-c demo-java clean

c:
	cc -std=c99 -Wall -Wextra -O2 -o c/demo c/demo.c

java:
	javac -d java/out java/Cdp.java java/Demo.java

demo-c: c
	sh demo.sh c/demo

demo-java: java
	sh demo.sh java -cp java/out Demo

demo: demo-c demo-java

clean:
	rm -f c/demo
	rm -rf java/out
