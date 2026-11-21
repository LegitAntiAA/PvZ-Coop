import unittest
from source_world_particles269 import initial_color

def source(mode=0,minimum='{r: 1, g: 1, b: 1, a: 1}',maximum='{r: 1, g: 0.6, b: 0, a: 1}'):
    return f'  minMaxState: {mode}\n  minColor: {minimum}\n  maxColor: {maximum}\n'

class InitialColorTests(unittest.TestCase):
    def test_constant_keeps_previous_max_color(self):
        self.assertEqual(initial_color(source()),[{'r':1.,'g':.6,'b':0.,'a':1.}])
    def test_two_colors_retain_original_endpoint_order(self):
        self.assertEqual(initial_color(source(2)),[{'r':1.,'g':1.,'b':1.,'a':1.},{'r':1.,'g':.6,'b':0.,'a':1.}])
    def test_hdr_endpoints_retained_before_native_clamp(self):
        self.assertEqual(initial_color(source(2,maximum='{r: 4, g: 2, b: 0, a: 0.5}'))[1]['r'],4)
    def test_time_gradient_still_rejected(self):
        with self.assertRaises(ValueError):initial_color(source(1))
    def test_two_gradients_still_rejected(self):
        with self.assertRaises(ValueError):initial_color(source(3))
    def test_nonfinite_rejected(self):
        with self.assertRaises(ValueError):initial_color(source(maximum='{r: NaN, g: 1, b: 1, a: 1}'))
    def test_missing_alpha_rejected(self):
        with self.assertRaises(ValueError):initial_color(source(maximum='{r: 1, g: 1, b: 1}'))
    def test_channel_bounds_rejected(self):
        for value in [-.001,64.001]:
            with self.subTest(value=value),self.assertRaises(ValueError):initial_color(source(maximum=f'{{r: {value}, g: 1, b: 1, a: 1}}'))

if __name__=='__main__':unittest.main()
