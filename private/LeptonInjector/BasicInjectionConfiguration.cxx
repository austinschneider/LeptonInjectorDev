
#include <BasicInjectionConfiguration.h>
#include <photospline/cinter/splinetable.h>

namespace LeptonInjector{

	BasicInjectionConfiguration::BasicInjectionConfiguration():
	events(1),
	energyMinimum(10*Constants::GeV),
	energyMaximum((1e9)*Constants::GeV),
	powerlawIndex(1.0),
	azimuthMinimum(0),
	azimuthMaximum(2*Constants::pi),
	zenithMinimum(0),
	zenithMaximum(Constants::pi),
	finalType1(Particle::ParticleType::MuMinus),
	finalType2(Particle::ParticleType::Hadrons),
	injectionRadius(1200*LeptonInjector::Constants::m),
	endcapLength(1200*LeptonInjector::Constants::m),
	cylinderRadius(1200*LeptonInjector::Constants::m),
	cylinderHeight(1200*LeptonInjector::Constants::m)
	{}

	
	void BasicInjectionConfiguration::setCrossSection(const photospline::splinetable<>& crossSection, const photospline::splinetable<>& totalCrossSection){
		splinetable_buffer buf;
		buf.size=0;
		//buf.mem_alloc=&malloc;
		//buf.mem_realloc=&realloc;

		// int result=writesplinefitstable_mem(&buf, &crossSection. );
		// auto& real_table=*static_cast<photospline::splinetable<>*>(crossSection->data);
		auto result_obj = crossSection.write_fits_mem();
		buf.data=result_obj.first;
		buf.size=result_obj.second;


		crossSectionBlob.resize(buf.size);
		std::copy((char*)buf.data,(char*)buf.data+buf.size,&crossSectionBlob[0]);
		free(buf.data);
		
		buf.size=0;
		result_obj = totalCrossSection.write_fits_mem();
		buf.data = result_obj.first;
		buf.size = result_obj.second;
		// result=writesplinefitstable_mem(&buf, &totalCrossSection);

		totalCrossSectionBlob.resize(buf.size);
		std::copy((char*)buf.data,(char*)buf.data+buf.size,&totalCrossSectionBlob[0]);
		free(buf.data);
	}


} // end namespace LeptonInjector