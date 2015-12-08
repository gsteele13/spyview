# Spyview

A data plotting and exploration program I wrote during my PhD: 

http://nsweb.tn.tudelft.nl/~gsteele/spyview/

I'm hosting it now on github, and have included some updates from github users, incluing a good first attempt by Ben to bundle it into a .app, and also from a github user "wakass" who cleaned up the c++ pointer casts to compile properly with clang (thanks wakass, whoever you are!).

# To Do list

IT’S ALIVE AGAIN! TODO list 2015!

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

# Changelog

I maintain a changelog on the website:

http://nsweb.tn.tudelft.nl/~gsteele/spyview/#news


