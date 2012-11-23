source /etc/ltsp/profiles/kicktoo-5.3.profile

# setting config vars
extra_packages ldm ltsp-client sysklogd ${PACKAGES}
rcadd sysklogd default
rcadd ltsp-client-setup boot
rcadd ltsp-client default


# Step control extra functions
post_unpack_stage_tarball() {
	# bind mounting portage and binary package dir
	mount_bind "/usr/portage" "${chroot_dir}/usr/portage"
	mount_bind "/usr/portage/packages/${ARCH}" "${chroot_dir}/usr/portage/packages"
	
	# bind mounting layman, for overlay packages
	# TODO: remove this mounting when the ltsp ebuilds are in the tree
	mount_bind "/var/lib/layman" "${chroot_dir}/var/lib/layman"

	# TODO: don't add this by default
	cat >> ${chroot_dir}/etc/portage/make.conf <<- EOF
	source /var/lib/layman/make.conf
	EOF

	cat > ${chroot_dir}/etc/fstab <<- EOF
	# DO NOT DELETE
	EOF
	
	# making sure ltsp-client 5.3 is not installed
	cat > ${chroot_dir}/etc/portage/package.mask <<- EOF
	>=net-misc/ltsp-client-5.3
	EOF

	# linking ltsp profile from overlay
	rm ${chroot_dir}/etc/portage/make.profile
	ln -s "/var/lib/layman/ltsp/profiles/default/linux/${MAIN_ARCH}/10.0/ltsp/" "${chroot_dir}/etc/portage/make.profile"
}

post_install_extra_packages() {
	# remove excluded packages
	for package in ${EXCLUDE}; do
		spawn_chroot "emerge --unmerge ${package}"
	done

	# remove possible dependencies of excluded
	spawn_chroot "emerge --depclean"
	
	# point /etc/mtab to /proc/mounts
	spawn "ln -sf /proc/mounts ${chroot_dir}/etc/mtab"

	# make sure these exist
	mkdir -p ${chroot_dir}/var/lib/nfs
	mkdir -p ${chroot_dir}/var/lib/pulse
	
	# required for openrc's bootmisc
	mkdir -p ${chroot_dir}/var/lib/misc
	
	# required in 5.2 clients
	touch ${chroot_dir}/etc/ltsp_chroot
}
