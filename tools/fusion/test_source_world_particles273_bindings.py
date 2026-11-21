import unittest
from source_world_particles269 import sprite_sequence

GUID='0123456789abcdef0123456789abcdef'

def uv(*entries):
    return '  sprites:\n'+''.join('  - sprite: '+entry+'\n'for entry in entries)+'  flipU: 0\n'

def ref(guid=GUID):
    return '{fileID: 21300000, guid: '+guid+', type: 2}'

class SpriteBindingsTests(unittest.TestCase):
    def test_all_original_frames_in_order(self):
        second='f'*32
        self.assertEqual(sprite_sequence(uv(ref(),ref(second),ref())),[GUID,second,GUID])

    def test_explicit_null_requires_engine_proof(self):
        with self.assertRaisesRegex(ValueError,'explicit null Sprite'):sprite_sequence(uv('{fileID: 0}'))

    def test_mixed_null_frame_must_not_be_silently_removed(self):
        for entries in [(ref(),'{fileID: 0}'),('{fileID: 0}',ref())]:
            with self.subTest(entries=entries),self.assertRaisesRegex(ValueError,'explicit null Sprite'):sprite_sequence(uv(*entries))

    def test_unresolved_or_malformed_frames_are_rejected(self):
        for entry in ['{fileID: 21300000}','{fileID: 123, guid: '+GUID+', type: 2}',ref().replace('type: 2','type: 3'),'not-a-reference','{fileID: 0','']:
            with self.subTest(entry=entry),self.assertRaises(ValueError):sprite_sequence(uv(ref(),entry))

    def test_empty_and_capacity_rejected(self):
        for entries in [(),(ref(),)*17]:
            with self.assertRaises(ValueError):sprite_sequence(uv(*entries))

    def test_full_supported_capacity_retained(self):
        self.assertEqual(sprite_sequence(uv(*(ref(),)*16)),[GUID]*16)

if __name__=='__main__':unittest.main()
