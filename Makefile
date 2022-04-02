ARCH?=x86_64
ARCH_ALT?=amd64
CROSS_COMPILE?=$(ARCH)-unknown-linux-gnu-

.PHONY: default
default: nomados-ext-$(ARCH).cpio.gz

.PHONY: test
test: nomados-ext-$(ARCH).cpio.gz dep/linux/arch/$(ARCH)/boot/bzImage
	qemu-system-$(ARCH) \
		-smp 2 \
		-m 4G \
		-netdev user,id=u1 -device e1000,netdev=u1 \
		-kernel dep/linux/arch/$(ARCH)/boot/bzImage \
		-initrd nomados-ext-$(ARCH).cpio.gz \
		-append "console=ttyS0 loglevel=8" \
		-nographic \
		--enable-kvm

nomados-ext-$(ARCH).cpio.gz: nomados-ext-$(ARCH).cpio
	cat nomados-ext-$(ARCH).cpio | gzip -9 > nomados-ext-$(ARCH).cpio.gz

nomados-ext-$(ARCH).cpio: initrd src/initrd/init
	(cd initrd ; find . | cpio --owner=root:root --format=newc -o > ../nomados-ext-$(ARCH).cpio)

initrd: rootfs.tar dep/sbase/tar src/initrd/init
	# Create directory structure
	mkdir -p initrd
	# Add init & tar
	cp src/initrd/init initrd/init
	cp dep/sbase/tar   initrd/tar
	# Load up rootfs
	cp rootfs.tar initrd/

rootfs.tar: rootfs
	tar cv -C rootfs $(shell ls rootfs/) > rootfs.tar

rootfs: dep/sdhcp/sdhcp dep/nomad/nomad src/rootfs/init
	# Create directory structure
	mkdir -p rootfs
	mkdir -p rootfs/etc
	mkdir -p rootfs/sbin
	# Add init
	cp src/rootfs/init rootfs/sbin/init
	./add-deps.sh rootfs/sbin/init rootfs
	# Add sdhcp
	cp dep/sdhcp/sdhcp rootfs/sbin/sdhcp
	./add-deps.sh rootfs/sbin/sdhcp rootfs
	# Add nomad
	mkdir -p rootfs/etc/nomad
	cp config/nomad.json rootfs/etc/nomad/init.json
	cp dep/nomad/nomad rootfs/sbin/nomad
	./add-deps.sh rootfs/sbin/nomad rootfs

dep/linux/arch/$(ARCH)/boot/bzImage: dep/linux
	(cd dep/linux ; $(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) allnoconfig)
	(cd dep/linux ; $(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) kvm_guest.config)
	# Bootable
	(cd dep/linux ; echo "CONFIG_BLK_DEV_INITRD=y" >> .config)
	(cd dep/linux ; echo "CONFIG_BLK_DEV_RAM=y"    >> .config)
	(cd dep/linux ; echo "CONFIG_TMPFS=y"          >> .config)
	(cd dep/linux ; echo "CONFIG_DEVTMPFS=y"       >> .config)
	# Multi-core support
	(cd dep/linux ; echo "CONFIG_SMP=y"            >> .config)
	# Network support
	(cd dep/linux ; echo "CONFIG_PACKET=y"         >> .config)
	(cd dep/linux ; echo "CONFIG_UNIX=y"           >> .config)
	(cd dep/linux ; echo "CONFIG_E1000=y"          >> .config)
	(cd dep/linux ; echo "CONFIG_E1000E=y"         >> .config)
	# Making nomad happy
	(cd dep/linux ; echo "CONFIG_BRIDGE=y"         >> .config)
	(cd dep/linux ; echo "CONFIG_BLK_CGROUP=y"     >> .config)
	(cd dep/linux ; echo "CONFIG_CGROUPS=y"        >> .config)
	(cd dep/linux ; echo "CONFIG_CGROUP_SCHED=y"   >> .config)
	(cd dep/linux ; yes '' | nice -n 5 $(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -j)

dep/linux:
	mkdir -p dep
	git clone --depth=1 git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git dep/linux

dep/sdhcp:
	mkdir -p dep
	git clone --depth=1 git://git.finwo.net/sdhcp dep/sdhcp

dep/sdhcp/sdhcp: dep/sdhcp
	(cd dep/sdhcp ; $(MAKE) CC=$(CROSS_COMPILE)gcc -j)
	(cd dep/sdhcp ; strip sdhcp)

dep/nomad:
	mkdir -p dep/nomad
	(cd dep/nomad ; wget https://releases.hashicorp.com/nomad/1.2.6/nomad_1.2.6_linux_$(ARCH_ALT).zip )

dep/nomad/nomad: dep/nomad
	(cd dep/nomad ; unzip -o nomad_1.2.6_linux_$(ARCH_ALT).zip )
	(cd dep/nomad ; strip nomad)

dep/sbase:
	mkdir -p dep
	git clone --depth=1 git://git.suckless.org/sbase dep/sbase

dep/sbase/tar: dep/sbase
	(cd dep/sbase ; $(MAKE) LDFLAGS=-static CC=$(CROSS_COMPILE)gcc -j tar)
	(cd dep/sbase ; strip tar)

src/initrd/init: src/initrd/init.c
	$(CROSS_COMPILE)gcc -O3 -static -s -Isrc/lib -o src/initrd/init src/lib/mount.c src/lib/spawn.c src/initrd/init.c
	strip src/initrd/init

src/rootfs/init: src/rootfs/init.c
	$(CROSS_COMPILE)gcc -O3 -s -Isrc/lib -o src/rootfs/init src/lib/mount.c src/lib/spawn.c src/rootfs/init.c
	strip src/rootfs/init

.PHONY: clean
clean:
	rm -rf initrd
	rm -rf rootfs
	rm -rf rootfs.tar
	rm -rf nomados-ext-x86_64.cpio
	rm -rf nomados-ext-x86_64.cpio.gz
	rm -rf dep/linux/arch/x86_64/boot/bzImage
	rm -rf src/initrd/init
	rm -rf src/rootfs/init
	(cd dep/linux && make clean || echo -n '')
	(cd dep/sdhcp && make clean || echo -n '')
	(cd dep/sbase && make clean || echo -n '')
