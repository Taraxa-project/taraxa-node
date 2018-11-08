#! /usr/bin/env python3
# implements algorand style account selection based
# on output from verifiable random function

from argparse import ArgumentParser
from functools import total_ordering

parser = ArgumentParser(description = 'Prints whether user was selected')
parser.add_argument('--balance', type = int, nargs = '+', default = [1], help = "Users' number of coins (decimal)")
parser.add_argument('--account', type = int, nargs = '+', default = [0], help = "Users' accounts (decimal, 0 <= account < 2**BITS)")
parser.add_argument('--hash', type = int, nargs = '+', default = [1], help = "Users' hashes produced from verifiable random function (decimal, 0 <= hash < 2**BITS)")
parser.add_argument('--bits', type = int, default = 1, help = 'Maximum number of bits in hash')
args = parser.parse_args()

# order coins by account (hash)
@total_ordering
class Account:
	def __init__(self, given_id, given_balance, given_hash):
		if not type(given_id) is int:
			exit('Account ID must be int')
		if not type(given_balance) is int:
			exit('Account balance must be int')
		if not type(given_hash) is int:
			exit('Hash must be int')
		self.id_ = given_id
		self.balance_ = given_balance
		self.hash_ = given_hash
	def __eq__(self, other):
		return self.id_ == other.id_
	def __lt__(self, other):
		return self.id_ < other.id_
	def __repr__(self):
		return '(' + str(self.id_) + ': ' + str(self.balance_) + ')'

if len(args.account) != len(args.balance):
	exit('Number of accounts and balances must match.')
if len(args.account) != len(args.hash):
	exit('Number of accounts and hashes must match.')

accounts = sorted(
	[Account(args.account[i], args.balance[i], args.hash[i])
		for i in range(len(args.account))]
)

total_coins = 0
for account in accounts:
	total_coins += account.balance_

# accounts whose (hash / 2**bits) falls within
# their (coin(s) / total coins) are selected
start, end = 0, 0
selected = []
for i in range(len(accounts)):
	# coin range
	end = start + accounts[i].balance_
	start_f, end_f = start / total_coins, end / total_coins
	# hash range
	hash_f = accounts[i].hash_ / 2**args.bits
	if hash_f >= start_f and hash_f < end_f:
		print('Selecting account', accounts[i].id_, 'because', start_f, '<=', hash_f, '<', end_f)
		selected.append(i)
	else:
		print('Not selecting account', accounts[i].id_, 'because', start_f, '<=', hash_f, '<', end_f, 'is false')
	start = end

print('Selected accounts:', end = ' ')
for i in selected:
	print(accounts[i], end = ', ')
print()
