all : aeq libasound_module_pcm_aeq.so

aeq : gui.c common.c common.h
	gcc -std=gnu99 -Wall -Wextra -O2 \
	 `pkg-config --cflags --libs gtk+-2.0 libnotify` \
	 -o aeq gui.c common.c

libasound_module_pcm_aeq.so : aeq.c common.c common.h
	gcc -std=gnu99 -Wall -Wextra -O2 -DPIC -fpic -shared \
	 `pkg-config --cflags --libs alsa` \
	 -Wl,-soname -Wl,libasound_module_pcm_aeq.so \
	 -o libasound_module_pcm_aeq.so aeq.c common.c

install :
	cp aeq /usr/bin/
	chmod 0755 /usr/bin/aeq
	cp libasound_module_pcm_aeq.so /usr/lib/alsa-lib/
	chmod 0755 /usr/lib/alsa-lib/libasound_module_pcm_aeq.so
	cp aeq.desktop /usr/share/applications/
	chmod 0644 /usr/share/applications/aeq.desktop
	mkdir -p /etc/aeq
	cp config /etc/aeq/
	chmod 0666 /etc/aeq/config
	update-desktop-database

uninstall :
	rm -f /usr/bin/aeq
	rm -f /usr/lib/alsa-lib/libasound_module_pcm_aeq.so
	rm -f /usr/share/applications/aeq.desktop
	rm -rf /etc/aeq
	update-desktop-database

clean :
	rm -f aeq libasound_module_pcm_aeq.so
