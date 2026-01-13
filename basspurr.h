//
// Bass "purring" effect - subharmonic generator with harmonic enhancement
// Inspired by classic bass tones (Beck - Loser, RATM - Bulls on Parade)
//
// Based on issue #18 discussion: the "purring" character comes from controlled
// redistribution of energy between sub, fundamental, and first harmonics.
// Uses envelope-tracked subharmonic generation + mild saturation for 2nd/3rd.
//

static struct {
	struct biquad_state input_lpf;    // Input lowpass for clean fundamental
	struct biquad_state sub_lpf;      // Subharmonic lowpass
	struct biquad_state harm_lpf;     // Harmonic content lowpass
	float sub_mix;                    // Octave-down level
	float harm_mix;                   // 2nd/3rd harmonic level
	float output;
	float last_sign;                  // For octave divider
	float sub_phase;                  // Subharmonic oscillator phase
	float envelope;                   // Envelope follower
} basspurr;

static inline void basspurr_init(float pot1, float pot2, float pot3, float pot4)
{
	// pot1: sub level (octave-down amount)
	// pot2: harmonic level (2nd/3rd harmonic saturation)
	// pot3: tone (overall brightness)
	// pot4: output level

	basspurr.sub_mix = pot1;
	basspurr.harm_mix = pot2;
	basspurr.output = 0.5 + pot4 * 0.5;
	basspurr.last_sign = 1;
	basspurr.sub_phase = 0;
	basspurr.envelope = 0;

	// Input filter - extract clean fundamental (80-300Hz typical bass range)
	biquad_init_lpf(&basspurr.input_lpf, 300, 0.707);

	// Subharmonic filter - keep only the octave-down content
	biquad_init_lpf(&basspurr.sub_lpf, 80, 0.707);

	// Harmonic filter - shape the saturation (controlled by tone pot)
	float harm_cutoff = 200 + pot3 * 2000;  // 200Hz to 2.2kHz
	biquad_init_lpf(&basspurr.harm_lpf, harm_cutoff, 0.707);

	fprintf(stderr, "basspurr:");
	fprintf(stderr, " sub=%g", basspurr.sub_mix);
	fprintf(stderr, " harmonics=%g", basspurr.harm_mix);
	fprintf(stderr, " tone=%g Hz", harm_cutoff);
	fprintf(stderr, " output=%g\n", basspurr.output);
}

static inline float basspurr_step(float in)
{
	// Envelope follower (for amplitude tracking)
	float abs_in = fabs(in);
	basspurr.envelope += 0.01 * (abs_in - basspurr.envelope);

	// Extract fundamental via lowpass
	float fundamental = biquad_step(&basspurr.input_lpf, in);

	// --- Subharmonic generation (octave-down) ---
	// Classic analog octaver: flip phase on each zero crossing
	float current_sign = (fundamental >= 0) ? 1 : -1;
	if (current_sign != basspurr.last_sign) {
		basspurr.sub_phase = -basspurr.sub_phase;  // Toggle
		if (basspurr.sub_phase == 0) basspurr.sub_phase = 1;
	}
	basspurr.last_sign = current_sign;

	// Subharmonic = square wave at half frequency, shaped by envelope
	float sub_raw = basspurr.sub_phase * basspurr.envelope;
	float sub = biquad_step(&basspurr.sub_lpf, sub_raw);

	// --- Harmonic enhancement (2nd/3rd via soft saturation) ---
	// Asymmetric clipping adds even harmonics (2nd)
	// Symmetric clipping adds odd harmonics (3rd)
	float driven = in * 2.0;
	float even = fabs(driven) - 0.5;  // Rectification for 2nd harmonic
	float odd = driven * driven * driven * 0.3;  // Cubic for 3rd harmonic
	float harmonics = biquad_step(&basspurr.harm_lpf, even + odd);

	// --- Mix: fundamental + sub-octave + harmonics ---
	float out = in
		+ sub * basspurr.sub_mix * 0.8
		+ harmonics * basspurr.harm_mix * 0.5;

	// Soft limit
	out = out / (1 + fabs(out));

	return out * basspurr.output;
}
