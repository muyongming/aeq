all : aeq libasound_module_pcm_aeq.so

aeq : gui.c common.c common.h
	gcc -std=gnu99 -Wall -Wextra -O2 -g \
	 `pkg-config --cflags --libs gtk+-2.0 libnotify` \
	 -o aeq gui.c common.c

libasound_module_pcm_aeq.so : aeq.c common.c common.h
	gcc -std=gnu99 -Wall -Wextra -O2 -g -DPIC -fpic -shared \
	 `pkg-config --cflags --libs alsa` \
	 -Wl,-soname -Wl,libasound_module_pcm_aeq.so \
	 -o libasound_module_pcm_aeq.so aeq.c common.c

install :
	mkdir -p $(DESTDIR)/usr/bin
	cp aeq $(DESTDIR)/usr/bin/
	chmod 0755 $(DESTDIR)/usr/bin/aeq
	mkdir -p $(DESTDIR)/usr/lib/alsa-lib
	rm -f $(DESTDIR)/usr/lib/alsa-lib/libasound_module_pcm_aeq.so  # delete then replace
	cp libasound_module_pcm_aeq.so $(DESTDIR)/usr/lib/alsa-lib/
	chmod 0755 $(DESTDIR)/usr/lib/alsa-lib/libasound_module_pcm_aeq.so
	mkdir -p $(DESTDIR)/usr/share/applications
	cp aeq.desktop $(DESTDIR)/usr/share/applications/
	chmod 0644 $(DESTDIR)/usr/share/applications/aeq.desktop
	mkdir -p $(DESTDIR)/etc/aeq
	cp config $(DESTDIR)/etc/aeq/
	chown root:audio $(DESTDIR)/etc/aeq/config
	chmod 0664 $(DESTDIR)/etc/aeq/config

uninstall :
	rm -f $(DESTDIR)/usr/bin/aeq
	rm -f $(DESTDIR)/usr/lib/alsa-lib/libasound_module_pcm_aeq.so
	rm -f $(DESTDIR)/usr/share/applications/aeq.desktop
	rm -rf $(DESTDIR)/etc/aeq

clean :
	rm -f aeq libasound_module_pcm_aeq.so
