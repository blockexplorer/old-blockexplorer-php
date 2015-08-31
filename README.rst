Blockexplorer now based on Insight
==================================

Blockexplorer.com is now based on BitPay's Insight. This old PHP code
no longer runs any part of the site. I'm keeping it around mostly 
for educational/reference purposes.

The new sources are here:

https://github.com/bitcoin-blockexplorer/

Here be dragons!
================

This version of blockexplorer.com is a raw work in progress that
probably needs special handling to get into working condition. Don't
expect it to just work, just yet. Handle with care. It may eat your
pets.

In its current state you'll probably have to know what you're doing to
get it to do anything.  If you can't figure it out on your own please
come back later. Eventually I plan on making it very easy to get a copy
of the site working but we're not there yet.

The code in this repository was originally written in a hurry by Michael
Marquardt AKA Theymos. Since Michael didn't have time to continue
working on blockexplorer.com he decided to pass the torch to Liraz.
That's me. Unfortunately I don't have a lot of free time either since I
spend most of it developing TurnKey Linux. So rather than doing
everything myself the plan was to focus on bootstrapping blockexplorer
into an open source project and get more people on board. I'd like
blockexplorer to become a neutral open source resource by the Bitcoin
community, for the Bitcoin community.

Rewrite in progress: what I've done so far
==========================================

Unfortunately, the original code was in a deeply troubled state. If you
want to see what I mean, checkout the Initial commit. I very much doubt
anyone would have had fun working on it in its original condition so I
volunteered myself for janitorial duty and have started a rewrite. It's
dirty work, but somebody has to do it!

What I've done so far:

- separated model/view/content
- eliminated most code repetition
- improved readability across the board
- added a templating system (smarty)
- created improved abstraction layers for caching, database and bitcoind
  RPC API
- moved various hardwired configurations that were interwoven in various
  spots in the code into a single configuration file
- eliminated use of gotos and globals
- fixed the most glaring performance issues

So far the code for the web application has been reduced from 6000 to
2500 lines of code.

I *nearly* finished the rewrite but then I ran out of time around May
2013 and had to get back to working on TurnKey (we had a release coming
out). I was planning on getting back to this in a few weeks, put
everything into working order and then release the code in working
condition. Unfortunately that didn't happen and continuing details have
convinced me it would be a better to just publish the incomplete
development code rather than continue to sit on it.

This way perhaps it can at least serve as a useful educational resource
for people interested in Bitcoin. OK, so maybe I'm also secretly hoping
someone will be interested in this enough to pick up the gauntlet and
make some progress until I get back to this.

Setup instructions
==================

Prerequisite: depends on an older custom patched bitcoind
---------------------------------------------------------

https://github.com/lirazsiri/blockexplorer-bitcoind

Setting up the database
-----------------------

explore.schema contains the current database schema. 

We grant write permissions to the blockupdate user (used to run the
blockupdate script) and read permissions to the www-data user - under
which we run the main blockexplorer web application.

Setting up the web application
------------------------------

1) Copy this repository to /var/www/blockexplorer.com.

2) You can use any web server so long as you properly configure it. I
   use lighttpd for the development version. You can find the lighttpd
   configuration I use in contrib/lighttpd-conf. This goes in
   /etc/lighttpd.

Configuration and initial setup of the major components
-------------------------------------------------------

1) htdocs/ contains the refactored blockexplorer.com web application
   which provides a web representation of the blockchain. 
   
   It needs read-only access to the database table where you will store
   the blockchain. It also communicates with bitcoind over RPC API for
   some queries.

   You can configure the the blockexplorer web application by editing
   htdocs/includes/config.inc.

1) bin/blockupdate*.php scripts connect to bitcoind over RPC API and
   copies blockchain data into the database. These scripts are intended
   to run from a cron job::

       BBE=/var/www/blockexplorer.com/ */1 * * * *	blockupdate
       $BBE/bin/blockupdate.php >> $BBE/logs/blockupdate.log */1 * * * *
       blockupdate $BBE/bin/blockupdate-testnet.php >>
       $BBE/logs/blockupdate-testnet.log
   
   These scripts haven't been rewritten yet so it still depends on
   pre-rewrite code that has all the configurations such as database
   table name hardwired. In other words, unlike the web application in
   htdocs/ the bin/ scripts don't take configurations from
   htdocs/includes/config.inc. 
   
   You'll need to identify the hardwired configurations and change them
   if you want blockupdate to work for you.

   If you haven't run blockupdate before, the first run will copy the
   entire blockchain into the database. This can take a while.
   Subsequent runs will only add new blocks however.

   These scripts need write access to the database table containing the
   blockchain. The current code needs 10X the amount of diskspace for
   the database version of the blockchain compared with bitcoin-qt.

   In other words, if bitcoin-qt's copy of the blockchain is 1GB,
   blockexplorer database table will require 10GB of diskspace.

   BTW, there's no inherent the copy of the blockchain in the database
   has to be 10X larger than the copy in bitcoind. This is just due to
   inefficiencies.

   For this reason however, it's recommended to run development versions
   of blockexplorer.com against testnet rather than mainnet. The testnet
   blockchain is much smaller.
