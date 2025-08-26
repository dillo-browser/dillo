# Detect beginning of leak message
/^(Direct|Indirect) leak/       { msg=msg "\n" $0; inleak=1; next }

# Ignore anything coming from libfontconfig.so
inleak && /libfontconfig.so/    { inleak=0; msg="" }

# Store leak line
inleak && /^ +#/                { msg=msg "\n" $0; next }

# End of the leak lines, print and finish
inleak                          { inleak=0; print msg; msg=""; nleaks++ }

# At exit count filtered leaks and fail accordingly
END { printf "\n==== Total leaks after filter: %d ====\n\n", nleaks; if (nleaks > 0) { exit 1 } }
