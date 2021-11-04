#!/usr/bin/env python3

import click
import os
import json
from eth_account import Account
from eth_utils.curried import keccak


@click.group()
def cli():
    """Taraxa Sign Utility"""
    pass


@cli.command()
@click.option('--key', default=None, help='Private key (optional)')
@click.option('--wallet', default=None, help='Wallet file path (optional)')
@click.argument('text', required=False)
def sign(key, wallet, text):
    """Sign text using private key or wallet"""

    if(key is None and wallet is None):
        click.echo(
            'Error: Please specify either a key or a wallet path (not both)')
        return

    if(key is not None and wallet is not None):
        click.echo(
            'Error: Please specify either a key or a wallet path (not both)')
        return

    if(key is not None and text is None):
        click.echo(
            'Error: Please specify a text to be signed')
        return

    private_key = None

    if(wallet is not None):
        if not os.path.isfile(wallet):
            click.echo('Error: Wallet file not found')
            return

        w = json.loads('{}')
        with open(wallet) as json_file:
            contents = json.load(json_file)
            w.update(contents)

        if not w['node_secret']:
            click.echo('Error: Private key not found in wallet file')
            return

        private_key = "0x{}".format(w['node_secret'])

        if(text is None):
            if not w['node_address']:
                click.echo('Error: Wallet address not found in wallet file')
                return

            text = "0x{}".format(w['node_address'])

    if(key is not None):
        private_key = key

    account = Account.from_key(private_key)
    sig = account.signHash(keccak(hexstr=text))
    sig_hex = sig.signature.hex()

    click.echo(sig_hex)


if __name__ == '__main__':
    cli()
