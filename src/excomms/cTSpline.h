#pragma once

namespace comms {

template<class Type>
class cTSpline {
public:
	cTSpline();
	virtual void Free();
	virtual void Copy(const cTSpline &Src);
	virtual int AddValue(const float Time, const Type &Value);
	int GetValuesCount() const { return m_Times.Count(); }
	virtual void RemoveAt(const int index);
	void ShiftTime(const float DeltaTime);
	
	const Type GetValue(const int index) const { return m_Values[index]; }
	void SetValue(const int index, const Type &Value) { m_Values[index] = Value; m_IsChanged = true; }
	float GetTime(const int index) const { return m_Times[index]; }

	float GetTimeStart() const { return m_Times[0]; }
	float GetTimeEnd() const { return m_IsClosed ? m_CloseTime : m_Times.GetLast(); }
	float GetDuration() const { return GetTimeEnd() - GetTimeStart(); }

	void Setup(const bool IsClosed, const float CloseTime) { m_IsClosed = IsClosed; m_CloseTime = CloseTime; m_IsChanged = true; }
	bool IsClosed() const { return m_IsClosed; }
	
	virtual const Type GetCurrentValue(const float Time) const;
	virtual const Type GetCurrent1stDerivative(const float Time) const;
	virtual const Type GetCurrent2ndDerivative(const float Time) const;
protected:
	mutable int m_CachedIndex;
	mutable bool m_IsChanged;
	bool m_IsClosed;
	float m_CloseTime;
	cList<float> m_Times;
	cList<Type> m_Values;
	int IndexForInsert(const float Time) const;
};

// cTSpline<Type>.ctor
template<class Type>
inline cTSpline<Type>::cTSpline() {
	m_CachedIndex = -1;
	m_IsChanged = false;
	m_IsClosed = false;
	m_CloseTime = 0.0f;
}

// cTSpline::Free
template<class Type>
inline void cTSpline<Type>::Free() {
	m_Times.Free();
	m_Values.Free();
	m_CachedIndex = -1;
	m_CloseTime = 0.0f;
	m_IsChanged = true;
	m_IsClosed = false;
}

template<class Type>
inline void cTSpline<Type>::Copy(const cTSpline<Type> &Src) {
    m_Times.Copy( Src.m_Times );
    m_Values.Copy( Src.m_Values );
	m_CachedIndex = -1;
	m_IsClosed = Src.m_IsClosed;
	m_CloseTime = Src.m_CloseTime;
	m_IsChanged = true;
}

template<class Type>
inline int cTSpline<Type>::AddValue(const float Time, const Type &Value) {
	int index = IndexForInsert(Time);
	m_Times.Insert(index, Time);
	m_Values.Insert(index, Value);
	m_IsChanged = true;
	return index;
}

template<class Type>
inline void cTSpline<Type>::RemoveAt(const int index) {
	m_Times.RemoveAt(index);
	m_Values.RemoveAt(index);
	m_IsChanged = true;
}

template<class Type>
inline void cTSpline<Type>::ShiftTime(const float DeltaTime) {
	int i;
	for(i = 0; i < m_Times.Count(); i++) {
		m_Times[i] += DeltaTime;
	}
	m_CloseTime += DeltaTime;
	m_IsChanged = true;
}

template<class Type>
inline const Type cTSpline<Type>::GetCurrentValue(const float Time) const {
	int index = IndexForInsert(Time);
	if(index >= m_Values.Count()) {
		return m_Values[m_Values.Count() - 1];
	} else {
		return m_Values[index];
	}
}

template<class Type>
inline const Type cTSpline<Type>::GetCurrent1stDerivative(const float Time) const {
	return m_Values[0] - m_Values[0];
}

template<class Type>
inline const Type cTSpline<Type>::GetCurrent2ndDerivative(const float Time) const {
	return m_Values[0] - m_Values[0];
}

// cTSpline<Type>::IndexForInsert
template<class Type>
inline int cTSpline<Type>::IndexForInsert(const float Time) const {
	if(m_CachedIndex >= 0 && m_CachedIndex <= m_Times.Count()) {
		if(m_CachedIndex == 0) {
			if(Time <= m_Times[m_CachedIndex]) {
				return m_CachedIndex;
			}
		} else if(m_CachedIndex == m_Times.Count()) {
			if(Time > m_Times[m_CachedIndex - 1]) {
				return m_CachedIndex;
			}
		} else if(Time > m_Times[m_CachedIndex - 1] && Time <= m_Times[m_CachedIndex]) {
			return m_CachedIndex;
		} else if(Time > m_Times[m_CachedIndex] &&
			(m_CachedIndex + 1 == m_Times.Count() || Time <= m_Times[m_CachedIndex + 1])) {
			m_CachedIndex++;
			return m_CachedIndex;
		}
	}

	m_CachedIndex = cMath::IndexForInsert(Time, m_Times.ToPtr(), m_Times.Count());
	return m_CachedIndex;
}

template<class Type>
class cSplineTCB : public cTSpline<Type> {
public:
	virtual void Free();
	virtual void Copy(const cSplineTCB<Type> &Src);
	virtual int AddValue(const float Time, const Type &Value, const float Tension = 0.0f, const float Continuity = 0.0f, const float Bias = 0.0f);
	virtual void RemoveAt(const int index);
	virtual const Type GetCurrentValue(const float Time, bool IsClosed = false, const float CloseTime = 0.0f) const;
	void SetConstantSpeed(const float TotalTime, const bool IsClosed);

	float GetTension(const int index) const { return m_Tension[index]; }
	float GetContinuity(const int index) const { return m_Continuity[index]; }
	float GetBias(const int index) const { return m_Bias[index]; }

    void SetTension(const int index, const float Tension) { m_Tension[index] = Tension; cTSpline<Type>::m_IsChanged = true; }
    void SetContinuity(const int index, const float Continuity) { m_Continuity[index] = Continuity; cTSpline<Type>::m_IsChanged = true; }
    void SetBias(const int index, const float Bias) { m_Bias[index] = Bias; cTSpline<Type>::m_IsChanged = true; }
	void PreCache() const;
private:
	cList<float> m_Tension;
	cList<float> m_Continuity;
	cList<float> m_Bias;

	struct CacheNode {
		float Time;
		Type Value;
		float Tension;
		float Continuity;
		float Bias;
		Type In;
		Type Out;
	};
	void AltCache() const;
	mutable cList<CacheNode> m_Cache;
};

typedef cSplineTCB<float> cSplineTCB1;
typedef cSplineTCB<cVec2> cSplineTCB2;
typedef cSplineTCB<cVec3> cSplineTCB3;
typedef cSplineTCB<cAngles> cSplineTCBA;

template<class Type>
inline void cSplineTCB<Type>::Free() {
    cTSpline<Type>::m_Times.Free();
	cTSpline<Type>::m_Values.Free();
	m_Tension.Free();
	m_Continuity.Free();
	m_Bias.Free();
	cTSpline<Type>::m_CachedIndex = -1;
	cTSpline<Type>::m_IsClosed = false;
	cTSpline<Type>::m_CloseTime = 0.0f;
	cTSpline<Type>::m_IsChanged = true;
}

template<class Type>
inline void cSplineTCB<Type>::Copy(const cSplineTCB<Type> &Src) {
    cTSpline<Type>::m_Times.Copy( Src.m_Times );
    cTSpline<Type>::m_Values.Copy( Src.m_Values );
    m_Tension.Copy( Src.m_Tension );
    m_Continuity.Copy( Src.m_Continuity );
    m_Bias.Copy( Src.m_Bias );
	cTSpline<Type>::m_CachedIndex = -1;
	cTSpline<Type>::m_IsClosed = Src.m_IsClosed;
	cTSpline<Type>::m_CloseTime = Src.m_CloseTime;
	cTSpline<Type>::m_IsChanged = true;
}


template<class Type>
inline int cSplineTCB<Type>::AddValue(const float Time, const Type &Value, const float Tension, const float Continuity, const float Bias) {
	int index = cTSpline<Type>::IndexForInsert(Time);
	cTSpline<Type>::m_Times.Insert(index, Time);
	cTSpline<Type>::m_Values.Insert(index, Value);
	m_Tension.Insert(index, Tension);
	m_Continuity.Insert(index, Continuity);
	m_Bias.Insert(index, Bias);
	cTSpline<Type>::m_IsChanged = true;
	return index;
}

template<class Type>
inline void cSplineTCB<Type>::RemoveAt(const int index) {
	cTSpline<Type>::m_Times.RemoveAt(index);
	cTSpline<Type>::m_Values.RemoveAt(index);
	m_Tension.RemoveAt(index);
	m_Continuity.RemoveAt(index);
	m_Bias.RemoveAt(index);
	cTSpline<Type>::m_IsChanged = true;
}

template<class Type>
inline void cSplineTCB<Type>::AltCache() const {
}

template<>
inline void cSplineTCB<cAngles>::AltCache() const {
	int i, k;
	float d;

	for(i = 0; i < m_Cache.Count(); i++) {
		m_Cache[i].Value.Normalize180();
		if(i > 0) {
			for(k = 0; k < 3; k++) {
				d = cMath::AngleNormalize180(m_Cache[i].Value[k] - m_Cache[i - 1].Value[k]);
				m_Cache[i].Value[k] = m_Cache[i - 1].Value[k] + d;
			}
		}
	}
}

template<class Type>
inline void cSplineTCB<Type>::PreCache() const {
	m_Cache.Free();
	cTSpline<Type>::m_IsChanged = false;
	if(cTSpline<Type>::m_Times.Count() < 1) {
		return;
	}

	CacheNode n;
	int i;
	for(i = 0; i < cTSpline<Type>::m_Times.Count(); i++) {
		n.Time = cTSpline<Type>::m_Times[i];
		n.Value = cTSpline<Type>::m_Values[i];
		n.Tension = m_Tension[i];
		n.Continuity = m_Continuity[i];
		n.Bias = m_Bias[i];
		m_Cache.Add(n);
	}
	
	if(cTSpline<Type>::m_IsClosed) {
		CacheNode NewLastByOne = m_Cache[0];
		CacheNode NewLast = m_Cache[1];
		CacheNode NewFirst = m_Cache.GetLast();
		NewFirst.Time = m_Cache[0].Time - (cTSpline<Type>::m_CloseTime - m_Cache.GetLast().Time);
		NewLastByOne.Time = cTSpline<Type>::m_CloseTime;
		NewLast.Time = cTSpline<Type>::m_CloseTime + (m_Cache[1].Time - m_Cache[0].Time);
		m_Cache.Insert(0, NewFirst);
		m_Cache.Add(NewLastByOne);
		m_Cache.Add(NewLast);
	}
	
	AltCache();

	if(m_Cache.Count() == 1) {
		return;
	}

	if(m_Cache.Count() == 2) {
		CacheNode &Start = m_Cache[0];
		CacheNode &End = m_Cache[1];
		Start.Out = End.In = cMath::TCBSetupStartOutEndIn(Start.Value, End.Value);
		return;
	}

	int iCur, iPrev, iNext;
	for(iCur = 0; iCur < m_Cache.Count(); iCur++) {
		iPrev = cMath::Clamp(iCur - 1, 0, m_Cache.Count() - 1);
		iNext = cMath::Clamp(iCur + 1, 0, m_Cache.Count() - 1);
		CacheNode &Cur = m_Cache[iCur];
		const CacheNode &Prev = m_Cache[iPrev];
		const CacheNode &Next = m_Cache[iNext];
		if(iPrev < iCur && iNext > iCur) {
			cMath::TCBSetupTimeAdj(Prev.Value, Cur.Value, Next.Value, Cur.In, Cur.Out, Prev.Time, Cur.Time, Next.Time, Cur.Tension, Cur.Continuity, Cur.Bias);
		} else if(iPrev == iCur) {
			cAssert(iNext > iCur);
			Cur.Out = cMath::TCBSetupStartOut(Cur.Value, Next.Value, Cur.Tension, Cur.Continuity, Cur.Bias);
		} else {
			cAssert(iNext == iCur);
			cAssert(iPrev < iCur);
			Cur.In = cMath::TCBSetupEndIn(Prev.Value, Cur.Value, Cur.Tension, Cur.Continuity, Cur.Bias);
		}
	}
}

template<class Type>
inline void cSplineTCB<Type>::SetConstantSpeed(const float TotalTime, const bool IsClosed) {
	if(cTSpline<Type>::m_Times.Count() <= 2) {
		return;
	}
	cTSpline<Type>::m_IsClosed = IsClosed;
	if(IsClosed) {
		cTSpline<Type>::m_CloseTime = TotalTime;
	}
	int i;
	for(i = 0; i < cTSpline<Type>::m_Times.Count(); i++) {
		cTSpline<Type>::m_Times[i] = float(i) * 0.01f;
	}
	PreCache();
	float TotalLength = 0.0f;
	cList<float> Ls;
	cSegBezier<Type> Seg;
	int iStart = IsClosed ? 1 : 0;
	int iEnd = IsClosed ? m_Cache.Count() - 2 : m_Cache.Count() - 1;
	for(i = iStart; i < iEnd; i++) {
		Seg.SetFromEnds(m_Cache[i].Value, m_Cache[i].Out, m_Cache[i + 1].Value, m_Cache[i + 1].In);
		Ls.Add(Seg.CalcLength());
		TotalLength += Ls.GetLast();
	}
	cTSpline<Type>::m_Times[0] = 0.0f;
	float AccumLength = 0.0f;
	for(i = 1; i < cTSpline<Type>::m_Times.Count(); i++) {
		AccumLength += Ls[i - 1];
		cTSpline<Type>::m_Times[i] = AccumLength / TotalLength * TotalTime;
	}
	cTSpline<Type>::m_IsChanged = true;
}

template<class Type>
inline const Type cSplineTCB<Type>::GetCurrentValue(const float Time, bool IsClosed, const float CloseTime) const {
	if(cTSpline<Type>::m_IsChanged) {
		PreCache();
		cTSpline<Type>::m_IsChanged = false;
	}

	if(cTSpline<Type>::m_Times.Count() && Time <= cTSpline<Type>::m_Times[0]) {
		return cTSpline<Type>::m_Values[0];
	}
	if(Time >= m_Cache.GetLast().Time) {
		return m_Cache.GetLast().Value;
	}
	
	int index = cMath::IndexForInsert(Time, &m_Cache[0].Time, m_Cache.Count(), sizeof(CacheNode));
	if(Time == m_Cache[index].Time) {
		return m_Cache[index].Value;
	}
	int i0 = index - 1;
	int i1 = index;
	
	float t = cMath::LerperClamp01(m_Cache[i0].Time, m_Cache[i1].Time, Time);
	
	cSegBezier<Type> Seg;
	Seg.SetFromEnds(m_Cache[i0].Value, m_Cache[i0].Out, m_Cache[i1].Value, m_Cache[i1].In);
	return Seg.GetValue(t);
}

} // comms
