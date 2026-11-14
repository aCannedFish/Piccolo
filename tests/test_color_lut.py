"""Check LUT layout and the shader's texel-center sampling convention numerically."""

import importlib.util
import math
from pathlib import Path
import random
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "generate_color_lut.py"
SPEC = importlib.util.spec_from_file_location("generate_color_lut", SCRIPT)
LUT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(LUT)


def sample_lut(pixels, size, color):
    """Model linear texture filtering and interpolation between blue slices."""
    red, green, blue = [min(max(value, 0.0), 1.0) * (size - 1) for value in color]
    lower_slice = math.floor(blue)
    upper_slice = min(lower_slice + 1, size - 1)

    def sample_slice(slice_index):
        # Normalized UVs convert back to these texel coordinates after the half-texel offset.
        x = slice_index * size + red
        y = green
        x0, y0 = math.floor(x), math.floor(y)
        x1 = min(x0 + 1, size * size - 1)
        y1 = min(y0 + 1, size - 1)
        result = [0.0, 0.0, 0.0]
        for column, weight_x in ((x0, 1.0 - (x - x0)), (x1, x - x0)):
            for row, weight_y in ((y0, 1.0 - (y - y0)), (y1, y - y0)):
                offset = (row * size * size + column) * 3
                for component in range(3):
                    result[component] += pixels[offset + component] / 255.0 * weight_x * weight_y
        return result

    lower = sample_slice(lower_slice)
    upper = sample_slice(upper_slice)
    weight = blue - lower_slice
    return [left * (1.0 - weight) + right * weight for left, right in zip(lower, upper)]


class ColorLutTests(unittest.TestCase):
    """Compare interpolated samples with analytic identity and warm transforms."""

    def test_identity_endpoints_and_slice_boundaries(self):
        pixels = LUT.make_lut(16, "identity")
        samples = [(red, green, blue) for red in (0, 1) for green in (0, 1) for blue in (0, 1)]
        samples.extend((1.0, 0.37, index / 15.0) for index in range(16))
        samples.extend((0.0, 0.37, (index + 0.5) / 15.0) for index in range(15))
        for color in samples:
            with self.subTest(color=color):
                for actual, expected in zip(sample_lut(pixels, 16, color), color):
                    self.assertAlmostEqual(actual, expected, places=6)

    def test_warm_transform_with_quantization(self):
        pixels = LUT.make_lut(16, "warm")
        random_source = random.Random(104)
        for _ in range(200):
            color = [random_source.random() for _ in range(3)]
            expected = LUT.grade_color(*color, "warm")
            for actual, target in zip(sample_lut(pixels, 16, color), expected):
                self.assertLessEqual(abs(actual - target), 0.5 / 255.0 + 1e-6)

    def test_out_of_range_color_is_clamped(self):
        pixels = LUT.make_lut(16, "identity")
        self.assertEqual(sample_lut(pixels, 16, (-1, 2, 1)), [0.0, 1.0, 1.0])

    def test_checked_in_lut_is_reproducible(self):
        import tempfile

        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "warm.png"
            LUT.write_png(output, 256, 16, LUT.make_lut(16, "warm"))
            asset = SCRIPT.parents[1] / "engine/asset/texture/lut/color_grading_warm.png"
            self.assertEqual(output.read_bytes(), asset.read_bytes())


if __name__ == "__main__":
    unittest.main()
