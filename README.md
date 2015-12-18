# Spyview

A data plotting and exploration program I wrote during my PhD: 

http://nsweb.tn.tudelft.nl/~gsteele/spyview/

I'm hosting it now on github, and have included some updates from github users, incluing a good first attempt by Ben to bundle it into a .app, and also from a github user "wakass" who cleaned up the c++ pointer casts to compile properly with clang (thanks wakass, whoever you are!).

# To Do list

IT’S ALIVE AGAIN! TODO list 2015!

Bugfixes:


Easy and useful:

- Keystroke for dismissing windows
- Button for loading printer settings from current directory?
- Start building a "remote interface" protocol to control spyview via stdin (Mario working on it?)

Harder and very useful:

- Add “plot waterfall option?” Maybe with a gui? And a gnuplot script output option?
- Come up with a plan on how to deploy on mac, maybe modify hard coding of shared file locations, or do a windows-style autodiscovery. Logical place would be to put the colormaps, etc, into the .app bundle. The preferences on mac should probably go in the user home directory though like on unix? 

Harder and maybe not so useful:

- Load data from images using FL_IMAGE class (low priority)
- Save colorized data as PNG using FL_IMAGE

# Done list

- Fix loading of data axes from meta.txt files? Somehow it doesn't work right now (at least with Mario's data?) It could be that a bugfix didn't make it through, or maybe Mario is using a broken python meta.txt generator (in which case it might be good to add a "broken meta.txt" button / option to support legacy files). Turned out it was just that the clang-fixed code was from a branch before I fixed some meta.txt loading bugs.

# Changelog

I maintain an old changelog on the website:

http://nsweb.tn.tudelft.nl/~gsteele/spyview/#news


