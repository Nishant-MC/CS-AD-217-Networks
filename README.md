CS-AD-217-Networks
==================

Repository that contains the source code for Jin U Bak &amp; Nishant MC's Bittorrent client implementation project

Steps for setting everything up and checking code correctness:

1) Download the .ZIP and unzip it.
2) cd into the directory and do "make".
3) We set up shortcuts for the peers. Open 2 terminal windows. To set
up peer A, write "./a-setup" and to set up peer B, write "./b-setup"
4) from peer A: GET tmp/B.chunks tmp/XXB.tar
5) from peer B: GET tmp/A.chunks tmp/XXA.tar
6) If you do diffs on on A vs. XXA and B vs. XXB you should see no difference
