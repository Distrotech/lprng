#!/bin/sh
echo ROUTER $0 "$@" 1>&2
cat <<'EOF'
dest t1
CClassTesting
end
EOF
