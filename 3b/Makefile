# NAME: Michael Huo, Tygan Zeng
# EMAIL: EMAIL
# ID: UID

.PHONY: clean dist default

default: lab3b

lab3b:
	@ln -s lab3b.sh lab3b

clean:
	@rm -f lab3b lab3b-UID.tar.gz

dist:
	@tar -czf lab3b-UID.tar.gz lab3b.py lab3b.sh Makefile README
