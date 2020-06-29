obj/mods/ssl/exb_ssl.o: src/exb/mods/ssl/exb_ssl.c src/exb/mods/ssl/exb_ssl_config_entry.h
	@mkdir -p '$(@D)'; \
	$(CC) -c $(CFLAGS) -o $@ $<
obj/libexb_mod_ssl.a: obj/mods/ssl/exb_ssl.o
	@mkdir -p '$(@D)'
	@ar rv $@ $^
