def restore(image_path: str, block_size: int, snapshot_dir: str='/snapshots'):
    parent = os.path.join(snapshot_dir, get_session_id(image_path))
    for f in os.listdir(parent):
        path = os.path.join(parent, f)
        try:
            subprocess.run(
            ['dd', f'if={path}', f'of={image_path}', f'bs={block_size}', f'seek={f}', 'conv=notrunc', 'status=none'],
                check=True, capture_output=True, text=True)
        except subprocess.CalledProcessError as e:
            s = f'restore failed for block={f}, got error {e}\n'
            s += f'stdout={e.stdout}\n'
            s += f'stderr={e.stderr}'
            print(s)
        except FileNotFoundError as e:
            print(f'cannot find command dd, got error {e}')

def get_session_id(image_path: str) -> str:
    pass