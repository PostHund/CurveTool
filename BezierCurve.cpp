#include "stdafx.h"
#include "BezierCurve.h"

#include "FMath.h"
#include "tge/drawers/DebugDrawer.h"

#include <algorithm>
#include <tge/engine.h>
#include <tge/drawers/LineDrawer.h>
#include <tge/graphics/GraphicsEngine.h>
#include <tge/primitives/LinePrimitive.h>

Forge::BezierCurve::BezierCurve()
{
	myCurvePoints.resize(4);
}

Tga::Vector3f& Forge::BezierCurve::operator[](uint8_t aIndex)
{
	return myCurvePoints[aIndex];
}

Tga::Vector3f Forge::BezierCurve::GetPointOnCurve(std::vector<Tga::Vector3f>& aCurvePoints, const float aPercentage)
{
	assert(aCurvePoints.size() == 4 && "If you bezier curve has more or less than 4 points, or you're working with splines, use the recursive function");

	const float safePercentage = std::clamp(aPercentage, 0.f, 1.f);

	const Tga::Vector3f firstPoint = FMath::Lerp(aCurvePoints[0], aCurvePoints[1], safePercentage);
	const Tga::Vector3f secondPoint = FMath::Lerp(aCurvePoints[1], aCurvePoints[2], safePercentage);
	const Tga::Vector3f thirdPoint = FMath::Lerp(aCurvePoints[2], aCurvePoints[3], safePercentage);
	const Tga::Vector3f fourthPoint = FMath::Lerp(firstPoint, secondPoint, safePercentage);
	const Tga::Vector3f fifthPoint = FMath::Lerp(secondPoint, thirdPoint, safePercentage);

	return FMath::Lerp(fourthPoint, fifthPoint, safePercentage);
}

Tga::Vector3f Forge::BezierCurve::GetPointOnCurve(const float aPercentage) const
{
	assert(myCurvePoints.size() == 4 && "If you bezier curve has more or less than 4 points, or you're working with splines, use the recursive function");

	const float safePercentage = std::clamp(aPercentage, 0.f, 1.f);

	const Tga::Vector3f firstPoint = FMath::Lerp(myCurvePoints[0], myCurvePoints[1], safePercentage);
	const Tga::Vector3f secondPoint = FMath::Lerp(myCurvePoints[1], myCurvePoints[2], safePercentage);
	const Tga::Vector3f thirdPoint = FMath::Lerp(myCurvePoints[2], myCurvePoints[3], safePercentage);
	const Tga::Vector3f fourthPoint = FMath::Lerp(firstPoint, secondPoint, safePercentage);
	const Tga::Vector3f fifthPoint = FMath::Lerp(secondPoint, thirdPoint, safePercentage);

	return FMath::Lerp(fourthPoint, fifthPoint, safePercentage);
}

void Forge::BezierCurve::DrawBezierCurve()
{
	assert(myCurvePoints.size() == 4 && "It's hard to draw a line with just one point. If your bezier curve has more or less than 4 points, or you're working with splines, use the recursive function");

	Tga::Vector3f previousPoint = myCurvePoints[0];

	for (int i = 1; i < myDrawFidelity; ++i)
	{
		float percentage = static_cast<float>(i) / static_cast<float>(myDrawFidelity - 1);
		Tga::Vector3f nextPoint = GetPointOnCurve(myCurvePoints, percentage);

		const Tga::Vector4f blendedColor = FMath::Lerp(myColorA, myColorB, percentage);

		Tga::LinePrimitive line;
		line.color = blendedColor;
		line.fromPosition = previousPoint;
		line.toPosition = nextPoint;
		Tga::LineDrawer& lineDrawer = Tga::Engine::GetInstance()->GetGraphicsEngine().GetLineDrawer();

		lineDrawer.Draw(line);

		previousPoint = nextPoint;
	}

}

void Forge::BezierCurve::DrawBasePoints(float pointRadius)
{
	Tga::DebugDrawer& debugDrawer = Tga::Engine::GetInstance()->GetDebugDrawer();

	float pointNumber = 3.f;
	for (auto& point : myCurvePoints)
	{
		const Tga::Vector4f blendedColor = FMath::Lerp(myColorB, myColorA, pointNumber / static_cast<float>(myCurvePoints.size()));
		debugDrawer.DrawSphere(point, pointRadius, blendedColor);
		--pointNumber;
	}

}

float Forge::BezierCurve::CalculateCurveLengthFastSample(int aSampleSubdivision)
{

	float totalLength = 0.f;

	Tga::Vector3f previousPoint = myCurvePoints[0];

	for (int i = 1; i < aSampleSubdivision; ++i)
	{
		float percentage = static_cast<float>(i) / static_cast<float>(aSampleSubdivision - 1);
		Tga::Vector3f nextPoint = GetPointOnCurve(myCurvePoints, percentage);

		totalLength += (nextPoint - previousPoint).Length();

		previousPoint = nextPoint;
	}

	return totalLength;
}
