#! /usr/bin/env python3
'''
Implements section 3.2 of eprint.iacr.org/2018/377.pdf as observer.
Each line read from file corresponds to votes for one step.
'''

from argparse import ArgumentParser
from os.path import exists

parser = ArgumentParser(description = 'Prints whether consensus is achieved')
parser.add_argument('--verbose', default = False, action = 'store_true')
parser.add_argument('--min-votes', type = int, default = 3, help = 'Minimum number of votes required for selection')
parser.add_argument('--file', type = str, required = True, help = "File with votes, one round per line, with each vote represented as A,B where A is vote's weight and B is vote's time")
args = parser.parse_args()


if args.min_votes < 1:
	exit('Minimum number of required votes must be at least 1')
if not exists(args.file):
	exit("Vote file doesn't exist")


'''
Consumes up to first line with votes in given list and
returns value that received at least args.min_votes votes
or TODO

Each vote is V,T,M where V is amount of votes, T time and V
is value that's voted for.
'''
def committee_vote(lines):
	if len(lines) == 0:
		return False

	result = dict()
	votes = [[int(v.split(',')[0]), int(v.split(',')[1]), int(v.split(',')[2])] for v in lines.pop(0).split()]
	total = 0
	for vote in votes:
		# 0 votes for empty block
		if vote[2] == 0:
			vote[2] = False
		if not vote[2] in result:
			result[vote[2]] = 0
		result[vote[2]] += vote[0]

	for v in result:
		if result[v] >= args.min_votes:
			return v
	return None


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
Section 3.2 of eprint.iacr.org/2018/377.pdf
Returns value for which agreement was reached
'''
def binary_ba(lines):
	# value proposal

	# filtering step

	# certifying step
	result = committee_vote(lines)
	if result != None and result != False:
		return result

	# 1st finishing step
	# insufficient votes from above
	return result

	# 2nd finishing step

block_nr = 1
while len(lines) > 0:
	block = binary_ba(lines)
	if block == False:
		block = 'Empty'
	print('Block', block_nr, ':', block)
	block_nr += 1
