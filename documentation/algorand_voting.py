#! /usr/bin/env python3
# implements algorand style voting for block

from argparse import ArgumentParser

#class Vote:
#	def __init__(self, given_number, given_time):
#		

# parses comma separated string and returns tuple of:
# number of votes, time in seconds
def vote_parser(vote):
	items = vote.split(',')
	return (int(items[0]), int(items[1]))

parser = ArgumentParser(description = 'Prints whether vote passes')
parser.add_argument('--verbose', default = False, action = 'store_true')
parser.add_argument('--min-votes', type = int, default = 3, help = 'Minimum number of votes required for selection')
parser.add_argument('--max-time', type = int, default = 3, help = "Time after which vote(s) don't count")
parser.add_argument('--vote', type = vote_parser, required = True, nargs = '+', help = "Vote represented as A,B where A is vote's weight and B is vote's time")
args = parser.parse_args()


if args.min_votes < 1:
	exit('Minimum number of required votes must be at least 1')
if args.max_time < 1:
	exit('Latest time for vote(s) must be at least 1')
if len(args.vote) < 1:
	exit('At least one vote required')

total = 0
for v in args.vote:
	if v[1] > args.max_time:
		continue
	total += v[0]

if total < args.min_votes:
	print('TIMEOUT')
else:
	print('PASS')
