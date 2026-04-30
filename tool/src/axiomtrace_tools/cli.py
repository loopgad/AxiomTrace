"""AxiomTrace decoder CLI entry point."""

import json
from pathlib import Path

import click

from axiomtrace_tools.decoder import decode_stream, find_dictionary


@click.command()
@click.argument('input', type=click.File('rb'))
@click.option('--dictionary', '-d', type=click.Path(exists=True, dir_okay=False),
              default=None, help='Path to dictionary.json (auto-searches if not provided)')
@click.option('--output', '-o', type=click.Choice(['text', 'json']), default='text',
              help='Output format')
def main(input, dictionary, output):
    """AxiomTrace binary frame decoder.

    Decode AXIOM binary frame files and output as text or JSON.
    """
    # Resolve dictionary: explicit -d takes priority, otherwise auto-search
    dict_path = None
    if dictionary:
        dict_path = Path(dictionary)
    else:
        dict_path = find_dictionary()

    if dict_path and dict_path.is_file():
        try:
            with open(dict_path, 'r', encoding='utf-8') as f:
                dictionary_data = json.load(f)
            click.echo(f"# Using dictionary: {dict_path}", err=True)
        except (json.JSONDecodeError, IOError) as e:
            click.echo(f"# Warning: Failed to load dictionary: {e}", err=True)
            dictionary_data = None
    else:
        dictionary_data = None

    raw = input.read()
    frames = decode_stream(raw)

    if output == 'json':
        click.echo(json.dumps(frames, indent=2))
    else:
        for f in frames:
            if 'error' in f:
                click.echo(f"[ERROR: {f['error']}]")
                continue
            click.echo(f"[{f['seq']:4d}] [{f['level']:5s}] mod=0x{f['module_id']:02X} evt=0x{f['event_id']:04X} payload={f['payload']}")


if __name__ == '__main__':
    main()
