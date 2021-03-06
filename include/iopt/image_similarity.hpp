#pragma once

#include <ostream>


struct Image;


namespace ImageSimilarity
{
	class Similarity
	{
	public:
		constexpr Similarity(float similarity) : m_value{ similarity }
		{
		}

		float GetValue() const { return m_value; }

	private:
		float m_value;
	};

	inline bool operator> (const Similarity& lhs, const Similarity& rhs)
	{
		return lhs.GetValue() > rhs.GetValue();
	}

	inline float operator- (const Similarity& lhs, const Similarity& rhs)
	{
		return lhs.GetValue() - rhs.GetValue();
	}

	std::ostream& operator<< (std::ostream& stream, Similarity similarity);

	Similarity ComputeSsim(const Image& referenceImage, const Image& compareImage);
}
