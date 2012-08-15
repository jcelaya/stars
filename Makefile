all: build/$(CONFIG)/Makefile
	$(MAKE) -C build/$(CONFIG)

build/$(CONFIG)/Makefile: build/$(CONFIG)
	cmake -E chdir build/$(CONFIG) cmake -G "Unix Makefiles" ../../ -DCMAKE_BUILD_TYPE:STRING=$(CONFIG)

build/$(CONFIG):
	mkdir -p build/$(CONFIG)

.PHONY: clean

clean:
	rm -fr build/$(CONFIG)
	