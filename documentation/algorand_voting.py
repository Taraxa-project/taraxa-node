#! /usr/bin/env python3
# implements algorand style voting for block

from argparse import ArgumentParser
from os.path import exists

parser = ArgumentParser(description = 'Prints whether consensus is achieved')
parser.add_argument('--verbose', default = False, action = 'store_true')
parser.add_argument('--max-steps', type = int, default = 11, help = 'Maximum number of steps to take to reach consensus')
parser.add_argument('--min-votes', type = int, default = 3, help = 'Minimum number of votes required for selection')
parser.add_argument('--max-time', type = int, default = 3, help = "Time after which vote(s) don't count")
parser.add_argument('--file', type = str, required = True, help = "File with votes, one round per line, with each vote represented as A,B where A is vote's weight and B is vote's time")
args = parser.parse_args()


if args.min_votes < 1:
	exit('Minimum number of required votes must be at least 1')
if args.max_time < 1:
	exit('Latest time for vote(s) must be at least 1')
if not exists(args.file):
	exit("Vote file doesn't exist")


'''
Consumes up to first line with votes in given list and
returns True if vote passes, False otherwise.
Implements ~CountVotes of algorand algorithm 5
'''
def committee_vote(lines):
	if len(lines) == 0:
		return False

	votes = [(int(v.split(',')[0]), int(v.split(',')[1])) for v in lines.pop(0).split()]
	total = 0
	for vote in votes:
		if vote[1] > args.max_time:
			continue
		total += vote[0]

	if total < args.min_votes:
		return False
	else:
		return True


consensus = None
lines = []

# load votes, remove comments
with open(args.file) as infile:
	for line in infile:
		line = line.strip()
		if line.startswith('#'):
			continue
		lines.append(line)


'''
Similar to algorand algorithm 8, returns True if
network agreed on non-empty block, False otherwise.
'''
def binary_ba(lines):
	print('processing', len(lines), 'lines')
	step = 0
	while step <= args.max_steps:
		step += 1

		consensus = committee_vote(lines)
		if not consensus:
			consensus = True
		else:
			# unused as this voting already included in lines
			'''
			for substep in range(step + 1, step + 4):
				...
			if step == 1:
				...
			'''
			return consensus
		step += 1

		consensus = committee_vote(lines)
		if not consensus:
			pass
		else:
			'''
			for substep in range(step + 1, step + 4):
				...
			'''
			return consensus
		step += 1

		consensus = committee_vote(lines)
		if not consensus:
			# "bsd" random selection
			consensus = True
		step += 1

print('Consensus:', consensus)
