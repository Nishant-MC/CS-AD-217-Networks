CS-AD-217-Networks
==================

Repository that contains the source code for Jin U Bak &amp; Nishant MC's Bittorrent client implementation project

Steps for setting everything up and checking code correctness:

- Download the .ZIP and unzip it.
- cd into the directory and do "make".
- We set up shortcuts for the peers. Open 2 terminal windows. To set
- peer A, write "./a-setup" and to set up peer B, write "./b-setup"
- from peer A: GET tmp/B.chunks tmp/XXB.tar
- from peer B: GET tmp/A.chunks tmp/XXA.tar
- If you do diffs on on A vs. XXA and B vs. XXB you should see no difference
