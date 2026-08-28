#pragma once

namespace comms {

	
class MeshCutter {
public:
	// should return signed value, if returns (value < 0), vertex will be cut
	virtual float GetCutWeight( const cVec3& p ) const = 0;
	virtual bool  NeedToNotifyAboutOpenVerts() const { return false; }
	virtual void  OnOpenVertex( int index, const cVec3& pos ) {
		// if NeedToNotifyAboutOpenVerts() return true this callback will be
		// called each time when vertex becomes open (or created) during cutting.
		// Vertex index is 'index', position is 'pos'
	}
};




// @todo Format accordance to 'LineDistanceCutter' with TODO.
class PlaneCutter: public MeshCutter {
public:
	comms::cPlane pl;
	cList<cVec3>* List;
	virtual float GetCutWeight(const cVec3& pt) const{
		return pl.Distance(pt);
	}
	virtual bool NeedToNotifyAboutOpenVerts() const{
		return List!=NULL;
	}
	virtual void OnOpenVertex(int Index,const cVec3& pos){
		if(List)List->Add(pos);
	}
};




// @todo fine  Rebase some methods to 'MeshCutter'.
// @todo fine  Move '*Cutter' to separate file.
// @todo fine  Approach '*Cutter' to standard STL (minimum: define
//       this as functor).
class LineDistanceCutter: public MeshCutter {
public:
	const comms::cLineF3  line;
	const float           radius;

	// @todo fine  Rebase to 'MeshCutter'.
	typedef ::std::pair< int /* index */, cVec3 /* coord */ >  openVert_t;
	typedef cList< openVert_t >  openVerts_t;

public:
	LineDistanceCutter( const comms::cLineF3& line, float radius, openVerts_t* list = NULL ) :
		line( line ),
		radius( radius ),
		list( list )
	{
		assert( line.IsValid() );
		assert( radius > 0 );
	}

	virtual ~LineDistanceCutter() {
	}

	virtual float GetCutWeight( const cVec3& p ) const {
		const cLineF3::distances_t  distances = line.Distances( p );
		for (int i = 0; i < distances.Count(); ++i) {
			const float  delta = distances[ i ] - radius;
			if (delta < 0) {
				// shrink this!
				return delta;
			}
		}
		return 1.0f;
	}

	virtual bool NeedToNotifyAboutOpenVerts() const {
		return (list != NULL);
	}

	virtual void OnOpenVertex( int index, const cVec3& coord ) {
		if (list) { list->Add( ::std::make_pair( index, coord ) ); }
	}

	const openVerts_t* GetList() const {
		return list;
	}

private:
	openVerts_t*  list;
};


} // comms
