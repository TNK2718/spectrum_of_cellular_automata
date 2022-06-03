#ifndef _CELLULAR_AUTOMATON_H_
#define _CELLULAR_AUTOMATON_H_

#include <cstdio>
#include <cstring>
#include <random>
#include <cmath>
#include <string>


const double PI = 3.14159265358979323846;

class CellularAutomaton {
public:
	CellularAutomaton()
		: CellNum_(0)
		, ySpectrum_(0)
		, HistoryNum_(0)
		, iteration_(0)
		, Rule_(0)
		, dt_(0.0)
		, loss_(0.001)
		, power_(NULL)
		, DFTRe_(NULL)
		, DFTIm_(NULL)
		, Cells_(NULL) {

	}

	CellularAutomaton(int xCells, int yCells, int HistoryNum,
		int rule = 0.01, double dt = 0.01)
		: CellNum_(xCells)
		, ySpectrum_(yCells)
		, HistoryNum_(HistoryNum)
		, iteration_(0)
		, Rule_(rule)
		, dt_(dt)
		, loss_(0.001)
		, power_(NULL)
		, DFTRe_(NULL)
		, DFTIm_(NULL)
		, Cells_(NULL) {

		allocateMemory();
	}

	CellularAutomaton(const CellularAutomaton &weq)
		: CellNum_(0)
		, ySpectrum_(0)
		, HistoryNum_(0)
		, iteration_(0)
		, Rule_(0)
		, dt_(0.0)
		, loss_(0.001)
		, power_(NULL)
		, DFTRe_(NULL)
		, DFTIm_(NULL)
		, Cells_(NULL) {
		this->operator=(weq);
	}

	virtual ~CellularAutomaton() {
		delete[] power_;
		delete[] DFTRe_;
		delete[] DFTIm_;
		delete[] Cells_;
	}

	CellularAutomaton & operator=(const CellularAutomaton &weq) {
		this->CellNum_ = weq.CellNum_;
		this->ySpectrum_ = weq.ySpectrum_;
		this->HistoryNum_ = weq.HistoryNum_;
		this->iteration_ = weq.iteration_;
		this->Rule_ = weq.Rule_;
		this->dt_ = weq.dt_;
		this->loss_ = weq.loss_;

		delete[] power_;
		delete[] DFTIm_;

		if (weq.power_ != NULL) {
			power_ = new double[CellNum_ * ySpectrum_];
			std::memcpy(power_, weq.power_, sizeof(double) * CellNum_ * ySpectrum_);
		}
		else {
			power_ = NULL;
		}

		if (weq.DFTRe_ != NULL) {
			DFTRe_ = new double[CellNum_];
			std::memcpy(DFTRe_, weq.DFTRe_, sizeof(double) * CellNum_ * ySpectrum_);
		}
		else {
			DFTRe_ = NULL;
		}

		if (weq.DFTIm_ != NULL) {
			DFTIm_ = new double[CellNum_];
			std::memcpy(DFTIm_, weq.DFTIm_, sizeof(double) * CellNum_ * ySpectrum_);
		}
		else {
			DFTIm_ = NULL;
		}

		return *this;
	}

	void setParams(int xCells, int yCells, int HistoryNum,
		int rule = 0, double dt = 0.01, double loss = 0.001) {
		this->CellNum_ = xCells;
		this->ySpectrum_ = yCells;
		this->HistoryNum_ = HistoryNum;
		this->Rule_ = rule;
		this->dt_ = dt;
		this->loss_ = loss;

		allocateMemory();
	}

	// initialize
	void start() {
		iteration_ = 0;
		std::random_device rnd;

		// Cells
		for (int i = 0; i < CellNum_; i++) {
			historySet(i, 0, rnd() % 2);
		}

		for (int i = 0; i < CellNum_; i++) {
			for (int j = 1; j < HistoryNum_; j++) {
				historySet(i, j, 0);
			}
		}

		// Spectrum
		for (int i = 0; i < CellNum_; i++) {
			for (int j = 0; j < ySpectrum_; j++) {
				DFTRe_[j * CellNum_ + i] = 0.0;
				DFTIm_[j * CellNum_ + i] = 0.0;
				set(i, j, 0.0);
			}
		}

		for (int i = 0; i < CellNum_; i++) {
			for (int k = 0; k < ySpectrum_; k++) {
				DFTRe_[k * CellNum_ + i] += historyGet(i, iteration_) * cos((2 * PI / HistoryNum_) * k * iteration_);
				DFTIm_[k * CellNum_ + i] += historyGet(i, iteration_) * (-sin((2 * PI / HistoryNum_) * k * iteration_));
			}
		}
	}

	// 
	void step() {
		if (iteration_ >= HistoryNum_ - 1) return;

		// update Cells
		for (int i = 0; i < CellNum_; i++) {
			// ŽüŠú“I‹«ŠEðŒ
			int state = 0;
			if (i == 0) {
				state += historyGet(i + 1, iteration_) << 0;
				state += historyGet(i, iteration_) << 1;
				state += historyGet(CellNum_ - 1, iteration_) << 2;
			}
			else if (i == CellNum_ - 1) {
				state += historyGet(0, iteration_) << 0;
				state += historyGet(i, iteration_) << 1;
				state += historyGet(i - 1, iteration_) << 2;
			}
			else {
				state += historyGet(i + 1, iteration_) << 0;
				state += historyGet(i, iteration_) << 1;
				state += historyGet(i - 1, iteration_) << 2;
			}

			// update
			historySet(i, iteration_ + 1, (Rule_ >> state) % 2);
		}

		iteration_++;

		// DFT
		for (int i = 0; i < CellNum_; i++) {
			for (int k = 0; k < ySpectrum_; k++) {
				DFTRe_[k * CellNum_ + i] += historyGet(i, iteration_) * cos((2 * PI / HistoryNum_) * k * iteration_);
				DFTIm_[k * CellNum_ + i] += historyGet(i, iteration_) * (-sin((2 * PI / HistoryNum_) * k * iteration_));
				set(i, k, log10(DFTRe_[k * CellNum_ + i] * DFTRe_[k * CellNum_ + i] + DFTIm_[k * CellNum_ + i] * DFTIm_[k * CellNum_ + i]));
			}
		}


		/*
		// rescaling		
		double max = 0.000000001;
		for (int i = 0; i < CellNum_; i++) {
			for (int k = 0; k < ySpectrum_; k++) {
				if (get(i, k) > max) max = get(i, k);
			}
		}
		for (int i = 0; i < CellNum_; i++) {
			for (int k = 0; k < ySpectrum_; k++) {
				set(i, k, 2.0 / max * (DFTRe_[k * CellNum_ + i] * DFTRe_[k * CellNum_ + i] + DFTIm_[k * CellNum_ + i] * DFTIm_[k * CellNum_ + i]));
			}
		}*/
	}

	void set(int x, int y, double height) {
		power_[y * CellNum_ + x] = height;
	}

	double get(int x, int y) const {
		return power_[y * CellNum_ + x];
	}

	double * const heights() const {
		return power_;
	}

	void historySet(int x, int y, double height) {
		Cells_[y * CellNum_ + x] = height;
	}

	int historyGet(int x, int y) const {
		return Cells_[y * CellNum_ + x];
	}

private:
	void allocateMemory() {
		delete[] power_;
		delete[] DFTRe_;
		delete[] DFTIm_;
		delete[] Cells_;

		power_ = new double[CellNum_ * ySpectrum_];
		DFTRe_ = new double[CellNum_ * ySpectrum_];
		DFTIm_ = new double[CellNum_ * ySpectrum_];
		Cells_ = new int[CellNum_ * HistoryNum_];
		std::memset(power_, 0, sizeof(double) * CellNum_ * ySpectrum_);
		std::memset(DFTRe_, 0, sizeof(double) * CellNum_ * ySpectrum_);
		std::memset(DFTIm_, 0, sizeof(double) * CellNum_ * ySpectrum_);
		std::memset(Cells_, 0, sizeof(int) * CellNum_ * HistoryNum_);
	}

	int CellNum_, ySpectrum_;
	int HistoryNum_;
	int iteration_;
	int Rule_;
	double dt_, loss_;
	double *power_;
	double *DFTRe_;
	double *DFTIm_;
	int *Cells_;
};

#endif  // _CELLULAR_AUTOMATON_H_
