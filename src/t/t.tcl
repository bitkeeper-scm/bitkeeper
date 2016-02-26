# Copyright 2005-2006,2009,2015-2016 BitMover, Inc

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This next test exposes a bug that occurs when Tcl is compiled with 
# GCC 2.7.2. If this regression fails, you need to switch compilers
# to GCC-2.95.3 or later.
echo $N Test Tcl/GCC compiler interaction \(AKA format bug\) ..........$NL
cd "$HERE"
echo '1 1 1 1' > expected
bk _tclsh 2>ERR >result <<'EOF'
    fconfigure stdout -translation lf

    set a 0xaaaaaaaa
    # Ensure $a and $b are separate objects
    set b 0xaaaa
    append b aaaa

    set result [expr {$a == $b}]
    format %08lx $b
    lappend result [expr {$a == $b}]

    set b 0xaaaa
    append b aaaa

    lappend result [expr {$a == $b}]
    format %08x $b
    lappend result [expr {$a == $b}]
    puts $result

EOF
checkfiles expected result
echo OK

if [ X$PLATFORM = XWIN32 ]; then

echo $N Checking for registry package ...............................$NL
bk _tclsh 2>ERR >result <<'EOF'
    package require registry 1.1
EOF
if [ $? -ne 0 ]; then echo failed ; exit 1; fi
echo OK

fi

echo $N Test that PCRE is working....................................$NL
cat <<EOF > pcre.l
void main()
{
	string s[] = split(/x/, "axbxc");
	if (defined(s[0])) {
		printf("PCRE is working.\n");
	} else {
		printf("PCRE not present.\n");
	}
}
EOF
bk _tclsh pcre.l 2>ERR >GOT
echo "PCRE is working." >WANT
cmpfiles WANT GOT
echo OK
