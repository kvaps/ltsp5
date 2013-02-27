PATTERN="(vmtoolsd.*vmusr|vmware-user)"

# Signal all running instances of the user daemon.
# Our pattern ensures that we won't touch the system daemon.
   pkill -$1 -f "$PATTERN"
   return 0
