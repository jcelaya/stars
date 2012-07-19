all: $(CONFIG)/Makefile
	$(MAKE) -C $(CONFIG)

$(CONFIG)/Makefile: $(CONFIG)
	cmake -E chdir $(CONFIG) cmake -G "Unix Makefiles" ../ -DCMAKE_BUILD_TYPE:STRING=$(CONFIG)

$(CONFIG):
	mkdir -p $(CONFIG)

.PHONY: clean

clean:
	rm -fr $(CONFIG)
	