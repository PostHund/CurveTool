#pragma once
#include "Vector3.h"
#include "Vector4.h"
#include <vector>

namespace Forge
{
	class BezierCurve
	{
	public:
		BezierCurve();
		BezierCurve(std::vector<Tga::Vector3f>& aCurvePoints) : myCurvePoints(aCurvePoints) {}
		~BezierCurve() = default;
		BezierCurve(const BezierCurve& other) = default;
		BezierCurve(BezierCurve& other) = default;
		BezierCurve& operator=(const BezierCurve& other) = default;
		Tga::Vector3f& operator[](uint8_t aIndex);

		void SetCurvePoints(const std::vector<Tga::Vector3f>& aCurvePoints) { myCurvePoints = aCurvePoints; }
		std::vector<Tga::Vector3f>& AccessCurvePoints() { return myCurvePoints; }

		static Tga::Vector3f GetPointOnCurve(std::vector<Tga::Vector3f>& aCurvePoints, const float aPercentage);
		Tga::Vector3f GetPointOnCurve(const float aPercentage) const;
		void DrawBezierCurve();
		void SetDebugDrawFidelity(int aDetailAmount) { myDrawFidelity = aDetailAmount; }
		void DrawBasePoints(float pointRadius = 50.f);

		void SetColorA(Tga::Vector4f aColor) { myColorA = aColor; }
		void SetColorB(Tga::Vector4f aColor) { myColorB = aColor; }
		Tga::Vector4f& GetColorA() { return myColorA; }
		Tga::Vector4f& GetColorB() { return myColorB; }

		float CalculateCurveLengthFastSample(int aSampleSubdivision = 1024);

	private:
		int myDrawFidelity = 16;
		std::vector<Tga::Vector3f> myCurvePoints;
		Tga::Vector4f myColorA = { 1.f, 0.f, 1.f, 1.f };
		Tga::Vector4f myColorB = { 0.f, 1.f, 0.f, 1.f };


	};
}
