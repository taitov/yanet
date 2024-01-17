deb:
	ls -l
	meson setup --prefix=/target_deb \
	            -Dtarget=release \
	            -Dyanet_config=release,firewall,l3balancer \
	            -Darch=corei7,broadwell,knl \
	            -Dversion_major=$$YANET_VERSION_MAJOR \
	            -Dversion_minor=$$YANET_VERSION_MINOR \
	            -Dversion_revision=$$YANET_VERSION_REVISION \
	            -Dversion_hash=$$YANET_VERSION_HASH \
	            -Dversion_custom=$$YANET_VERSION_CUSTOM \
	            build_deb
