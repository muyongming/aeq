all : aeq libasound_module_pcm_aeq.so

aeq : gui.c common.h
	gcc -std=gnu99 -Wall -Wextra -O2 \
	 `pkg-config --cflags --libs gtk+-3.0` \
	 -o aeq gui.c

libasound_module_pcm_aeq.so : aeq.c common.h
	gcc -std=gnu99 -Wall -Wextra -O2 -DPIC -fpic -shared \
	 `pkg-config --cflags --libs alsa` \
	 -Wl,-soname -Wl,libasound_module_pcm_aeq.so \
	 -o libasound_module_pcm_aeq.so aeq.c

install :
	cp aeq /usr/bin/
	chmod 0755 /usr/bin/aeq
	cp libasound_module_pcm_aeq.so /usr/lib/alsa-lib/
	chmod 0755 /usr/lib/alsa-lib/libasound_module_pcm_aeq.so
	cp aeq.desktop /usr/share/applications/
	chmod 0644 /usr/share/applications/aeq.desktop
	update-desktop-database

uninstall :
	rm -f /usr/bin/aeq
	rm -f /usr/lib/alsa-lib/libasound_module_pcm_aeq.so
	rm -f /usr/share/applications/aeq.desktop
	update-desktop-database

clean :
	rm -f aeq libasound_module_pcm_aeq.so
